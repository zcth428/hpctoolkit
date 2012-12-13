#include <stdio.h>
#include <jvmti.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/time.h>

#include "files.h"
#include <fnbounds/fnbounds_java.h>
#include "safe-sampling.h"

#include "opagent.h"

typedef void fct_add_interval(void*,void*);

static int debug = 0;
static int can_get_line_numbers = 1;
static struct timeval time_start;

static op_agent_t agent_hdl;

/**
 * Handle an error or a warning, return 0 if the checked error is 
 * JVMTI_ERROR_NONE, i.e. success
 */
static int handle_error(jvmtiError err, char const * msg, int severe)
{
	if (err != JVMTI_ERROR_NONE) {
		fprintf(stderr, "%s: %s, err code %i\n",
			severe ? "Error" : "Warning", msg, err);
	}
	return err != JVMTI_ERROR_NONE;
}


static struct debug_line_info *
create_debug_line_info(jint map_length, jvmtiAddrLocationMap const * map,
		       jint entry_count, jvmtiLineNumberEntry* table_ptr,
		       char const * source_filename)
{
	struct debug_line_info * debug_line;
	int i, j;
	if (debug) {
		fprintf(stderr, "Source %s, l: %d, addr: %p - %p\n", source_filename, map_length, 
			map[0].start_address, map[map_length-1].start_address);
	}

	debug_line = calloc(map_length, sizeof(struct debug_line_info));
	if (!debug_line)
		return 0;

	for (i = 0; i < map_length; ++i) {
		/* FIXME: likely to need a lower_bound on the array, but
		 * documentation is a bit obscure about the contents of these
		 * arrray
		 **/
		for (j = 0; j < entry_count - 1; ++j) {
			if (table_ptr[j].start_location > map[i].location)
				break;
		}
		debug_line[i].vma = (unsigned long)map[i].start_address;
		debug_line[i].lineno = table_ptr[j].line_number;
		debug_line[i].filename = source_filename;
	}

	if (debug) {
		for (i = 0; i < map_length; ++i) {
			fprintf(stderr, "%d\t-> %lx %s:%d\t\n", i, debug_line[i].vma,
				debug_line[i].filename,debug_line[i].lineno);
		}
		fprintf(stderr, "\n");
		fflush(stderr);
	}
	
	return debug_line;
}


/*
 * Sent when a method is compiled and loaded into memory by the VM. 
 * If it is unloaded, the CompiledMethodUnload event is sent. 
 * If it is moved, the CompiledMethodUnload event is sent, followed by a new CompiledMethodLoad event. 
 *
 * Note that a single method may have multiple compiled forms, and that this event will be sent for each form. 
 * Note also that several methods may be inlined into a single address range, 
 * and that this event will be sent for each method. 
 * 
 */
static void JNICALL cb_compiled_method_load(jvmtiEnv * jvmti,
	jmethodID method, jint code_size, void const * code_addr,
	jint map_length, jvmtiAddrLocationMap const * map,
	void const * compile_info)
{
	jclass declaring_class;
	char * class_signature = NULL;
 	char * method_name = NULL;
 	char * method_signature = NULL;
	jvmtiLineNumberEntry* table_ptr = NULL;
	char * source_filename = NULL;
	struct debug_line_info * debug_line = NULL;
 	jvmtiError err;

	/************* hpcrun critical section ************/
  	if (!hpcrun_safe_enter()) {
    		return;
  	}

	/* 
 	 * check if we can access to functions in hpcrun.so
 	 *  if dlopen fails or the function is not found, we just do nothing
 	 *  
 	 * */

	/* shut up compiler warning */
	compile_info = compile_info;

	err = (*jvmti)->GetMethodDeclaringClass(jvmti, method,
						&declaring_class);
	if (handle_error(err, "GetMethodDeclaringClass()", 1))
		goto cleanup2;

	if (can_get_line_numbers && map_length && map) {
		jint entry_count;

		err = (*jvmti)->GetLineNumberTable(jvmti, method,
						   &entry_count, &table_ptr);
		if (err == JVMTI_ERROR_NONE) {
			err = (*jvmti)->GetSourceFileName(jvmti,
				declaring_class, &source_filename);
			if (err ==  JVMTI_ERROR_NONE) {
				debug_line =
					create_debug_line_info(map_length, map,
						entry_count, table_ptr,
						source_filename);
			 
			} else if (err != JVMTI_ERROR_ABSENT_INFORMATION) {
				handle_error(err, "GetSourceFileName()", 1);
			}
		} else if (err != JVMTI_ERROR_NATIVE_METHOD &&
			   err != JVMTI_ERROR_ABSENT_INFORMATION) {
			handle_error(err, "GetLineNumberTable()", 1);
		}
	}

	err = (*jvmti)->GetClassSignature(jvmti, declaring_class,
					  &class_signature, NULL);
	if (handle_error(err, "GetClassSignature()", 1))
		goto cleanup1;

	err = (*jvmti)->GetMethodName(jvmti, method, &method_name,
				      &method_signature, NULL);
	if (handle_error(err, "GetMethodName()", 1))
		goto cleanup;

	if (debug) {
                struct timeval tv;
                if (gettimeofday(&tv, NULL)) {
                        fprintf(stderr,"gettimeofday fail");
                }

		fprintf(stderr, "load: t=%ld, declaring_class=%p, class=%s, "
			"method=%s, signature=%s, addr=%p, size=%i \n",  tv.tv_usec,
			declaring_class, class_signature, method_name,
			method_signature, code_addr, code_size);
	}

	{
	int cnt = strlen(method_name) + strlen(class_signature) +
		strlen(method_signature) + 2;
	char buf[cnt];
	strncpy(buf, class_signature, cnt - 1);
	strncat(buf, method_name, cnt - strlen(buf) - 1);
	strncat(buf, method_signature, cnt - strlen(buf) - 1);
	const void *code_addr_end = code_addr + code_size;
	hpcjava_add_address_interval(code_addr, code_addr_end);

        if (op_write_native_code(agent_hdl, buf,
                                 (uint64_t)(uintptr_t) code_addr,
                                 code_addr, code_size)) {
                perror("Error: op_write_native_code()");
                goto cleanup;
        }
	}

        if (debug_line)
                if (op_write_debug_line_info(agent_hdl, code_addr, map_length,
                                             debug_line))
                        perror("Error: op_write_debug_line_info()");

cleanup:
	(*jvmti)->Deallocate(jvmti, (unsigned char *)method_name);
	(*jvmti)->Deallocate(jvmti, (unsigned char *)method_signature);
cleanup1:
	(*jvmti)->Deallocate(jvmti, (unsigned char *)class_signature);
	(*jvmti)->Deallocate(jvmti, (unsigned char *)table_ptr);
	(*jvmti)->Deallocate(jvmti, (unsigned char *)source_filename);
cleanup2:
	free(debug_line);
	
	/************* end hpcrun critical section ************/
  	hpcrun_safe_exit();
}


/**
 * Sent when a compiled method is unloaded from memory. 
 * This event might not be sent on the thread which performed the unload. 
 * This event may be sent sometime after the unload occurs, 
 * 	but will be sent before the memory is reused by a newly generated compiled method. 
 * This event may be sent after the class is unloaded. 
 */
static void JNICALL cb_compiled_method_unload(jvmtiEnv * jvmti_env,
	jmethodID method, void const * code_addr)
{
	/************* hpcrun critical section ************/
	if (!hpcrun_safe_enter()) {
    		return 0;
  	}

	/* shut up compiler warning */
	jvmti_env = jvmti_env;
	method = method;

	if (debug)
		fprintf(stderr, "unload: addr=%p\n", code_addr);
        if (op_unload_native_code(agent_hdl, (uint64_t)(uintptr_t) code_addr))
                perror("Error: op_unload_native_code()");

	/************* end hpcrun critical section ************/
	hpcrun_safe_exit();
}

/**
 * Sent when a component of the virtual machine is generated dynamically. 
 * This does not correspond to Java programming language code that is compiled--see CompiledMethodLoad. 
 * This is for native code--for example, an interpreter that is generated differently 
 * 	depending on command-line options.
 *
 * Note that this event has no controlling capability. 
 * If a VM cannot generate these events, it simply does not send any.
 *
 * These events can be sent after their initial occurrence with GenerateEvents. 
 */
static void JNICALL cb_dynamic_code_generated(jvmtiEnv * jvmti_env,
	char const * name, void const * code_addr, jint code_size)
{
	/************* hpcrun critical section ************/
	if (!hpcrun_safe_enter()) {
    		return 0;
  	}

	/* shut up compiler warning */
	jvmti_env = jvmti_env;

	/* adding the interval to the java ui tree
 	 */ 	 
	const void *code_addr_end = code_addr + code_size;
	hpcjava_add_address_interval(code_addr, code_addr_end);

	if (debug) {
		fprintf(stderr, "dyncode: name=%s, addr=%p, size=%i \n",
			name, code_addr, code_size); 
	}
	if (op_write_native_code(agent_hdl, name,
                                 (uint64_t)(uintptr_t) code_addr,
                                 code_addr, code_size))
                perror("Error: op_write_native_code()");
	
	/************* end hpcrun critical section ************/
	hpcrun_safe_exit();
}

/***
 * The VM will start the agent by calling this function. It will be called early enough in VM initialization that:
 *
 *     system properties may be set before they have been used in the start-up of the VM
 *     the full set of capabilities is still available (note that capabilities that 
 *     		configure the VM may only be available at this time
 *     			--see the Capability function section)
 *     no bytecodes have executed
 *     no classes have been loaded
 *     no objects have been created
 *
 */
JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM * jvm, char * options, void * reserved)
{
	jint rc;
	jvmtiEnv * jvmti = NULL;
	jvmtiEventCallbacks callbacks;
	jvmtiCapabilities caps;
	jvmtiJlocationFormat format;
	jvmtiError error;
	jint res = 0;
	char *dir_collect = getenv("HPCRUN_JAVA_DATA_DIR");

	/************* hpcrun critical section ************/
	if (!hpcrun_safe_enter()) {
    		return 0;
  	}

	/* shut up compiler warning */
	reserved = reserved;

	hpcjava_interval_tree_init();

	if (options && !strcmp("debug", options))
		debug = 1;

	if (debug)
		fprintf(stderr, "jvmti hpcjava: agent activated\n");

        gettimeofday(&time_start, NULL);

	if (dir_collect == NULL)
		dir_collect = "/tmp/";

	char *hpc_dir = hpcrun_get_directory();
	printf("hpc_dir: %s\n", hpc_dir );
        agent_hdl = op_open_agent(hpc_dir);
        if (!agent_hdl) {
                perror("Error: op_open_agent()");
                res = -1;
		goto finalize;
        }

	rc = (*jvm)->GetEnv(jvm, (void *)&jvmti, JVMTI_VERSION_1);
	if (rc != JNI_OK) {
		fprintf(stderr, "Error: GetEnv(), rc=%i\n", rc);
		res = -1;
		goto finalize;
	}

	memset(&caps, '\0', sizeof(caps));
	caps.can_generate_compiled_method_load_events = 1;
	error = (*jvmti)->AddCapabilities(jvmti, &caps);
	if (handle_error(error, "AddCapabilities()", 1)) {
		res = -1;
		goto finalize;
	}

	/* FIXME: settable through command line, default on/off? */
	error = (*jvmti)->GetJLocationFormat(jvmti, &format);
	if (!handle_error(error, "GetJLocationFormat", 1) &&
	    format == JVMTI_JLOCATION_JVMBCI) {
		memset(&caps, '\0', sizeof(caps));
		caps.can_get_line_numbers = 1;
		caps.can_get_source_file_name = 1;
		error = (*jvmti)->AddCapabilities(jvmti, &caps);
		if (!handle_error(error, "AddCapabilities()", 1))
			can_get_line_numbers = 1;
	}

	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.CompiledMethodLoad = cb_compiled_method_load;
	callbacks.CompiledMethodUnload = cb_compiled_method_unload;
	callbacks.DynamicCodeGenerated = cb_dynamic_code_generated;

	error = (*jvmti)->SetEventCallbacks(jvmti, &callbacks,
					    sizeof(callbacks));
	if (handle_error(error, "SetEventCallbacks()", 1)) {
		res = -1;
		goto finalize;
	}

	error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
			JVMTI_EVENT_COMPILED_METHOD_LOAD, NULL);
	if (handle_error(error, "SetEventNotificationMode() "
			 "JVMTI_EVENT_COMPILED_METHOD_LOAD", 1)) {
		res = -1;
		goto finalize;
	}
	error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
			JVMTI_EVENT_COMPILED_METHOD_UNLOAD, NULL);
	if (handle_error(error, "SetEventNotificationMode() "
			 "JVMTI_EVENT_COMPILED_METHOD_UNLOAD", 1)) {
		res = -1;
		goto finalize;
	}
	error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
			JVMTI_EVENT_DYNAMIC_CODE_GENERATED, NULL);
	if (handle_error(error, "SetEventNotificationMode() "
			 "JVMTI_EVENT_DYNAMIC_CODE_GENERATED", 1)) {
		res = -1;	
		goto finalize;
	}

finalize:
	/************* end hpcrun critical section ************/
  	hpcrun_safe_exit();

	return res;
}


JNIEXPORT void JNICALL Agent_OnUnload(JavaVM * jvm)
{
        struct timeval time_end;

	/************* hpcrun critical section ************/
	if (!hpcrun_safe_enter()) {
    		return 0;
  	}

	/* shut up compiler warning */
	jvm = jvm;

	hpcjava_delete_ui();
	        if (op_close_agent(agent_hdl))
                perror("Error: op_close_agent()");

        gettimeofday(&time_end, NULL);
        printf("Time range jvmti: %d - %d \n", time_start.tv_sec, time_end.tv_sec );
        fflush(stdout);

	/************* end hpcrun critical section ************/
  	hpcrun_safe_exit();
}