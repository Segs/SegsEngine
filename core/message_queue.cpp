/*************************************************************************/
/*  message_queue.cpp                                                    */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "message_queue.h"

#include "core/callable_method_pointer.h"
#include "core/core_string_names.h"
#include "core/external_profiler.h"
#include "core/object_db.h"
#include "core/object.h"
#include "core/os/mutex.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/script_language.h"
#include "core/string_utils.h"

#define STACK_DEPTH 3

namespace {
SafeNumeric<int> null_object_calls {0};
}

MessageQueue *MessageQueue::singleton = nullptr;

MessageQueue *MessageQueue::get_singleton() {

    return singleton;
}

Error MessageQueue::push_call(GameEntity p_id, eastl::function<void()> p_method) {

    _THREAD_SAFE_METHOD_;
    constexpr size_t room_needed = sizeof(Message);

    if ((buffer_end + room_needed) >= buffer_size) {
        String type;
        auto obj = object_for_entity(p_id);
        if (obj) {
            type = obj->get_class();
        }
        print_line(String("Failed ::function call: ") + type + ": target ID: " + ::to_string(entt::to_integral(p_id)));
        statistics();
        ERR_FAIL_V_MSG(ERR_OUT_OF_MEMORY, "Message queue out of memory. Try increasing 'memory/limits/message_queue/max_size_kb' in project settings.");
    }
    TRACE_ALLOC_NS(&buffer[buffer_end],room_needed,STACK_DEPTH,"MessageQueueAlloc");
    Message *msg = memnew_placement(&buffer[buffer_end], Message);
    msg->args = 0;
    msg->callable = Callable(memnew_args(FunctorCallable,p_id,p_method));
    msg->type = TYPE_CALL;

    buffer_end += sizeof(Message);
    return OK;
}
Error MessageQueue::push_call(GameEntity p_id, const StringName &p_method, const Variant **p_args, int p_argcount, bool p_show_error) {

    return push_callable(Callable(p_id, p_method), p_args, p_argcount, p_show_error);
}

Error MessageQueue::push_call(GameEntity p_id, const StringName &p_method, VARIANT_ARG_DECLARE) {

    VARIANT_ARGPTRS

    int argc = 0;

    for (const Variant *arg_ptr : argptr) {
        if (arg_ptr->get_type() == VariantType::NIL) {
            break;
        }
        argc++;
    }

    return push_call(p_id, p_method, argptr, argc, false);
}

Error MessageQueue::push_callable(const Callable& p_callable, const Variant** p_args, int p_argcount, bool p_show_error) {
    _THREAD_SAFE_METHOD_;

    int room_needed = sizeof(Message) + sizeof(Variant) * p_argcount;

    if ((buffer_end + room_needed) >= buffer_size) {
        print_line("Failed method: " + (String)p_callable);
        statistics();
        ERR_FAIL_V_MSG(ERR_OUT_OF_MEMORY, "Message queue out of memory. Try increasing 'memory/limits/message_queue/max_size_kb' in project settings.");
    }

    TRACE_ALLOC_NS(&buffer[buffer_end],room_needed,STACK_DEPTH,"MessageQueueAlloc");

    Message* msg = memnew_placement(&buffer[buffer_end], Message);
    msg->args = p_argcount;
    msg->callable = p_callable;
    msg->type = TYPE_CALL;
    if (p_show_error) {
        msg->type |= FLAG_SHOW_ERROR;
    }

    buffer_end += sizeof(Message);

    for (int i = 0; i < p_argcount; i++) {
        Variant* v = memnew_placement(&buffer[buffer_end], Variant);
        buffer_end += sizeof(Variant);
        *v = *p_args[i];
    }

    return OK;
}
Error MessageQueue::push_callable(const Callable& p_callable, VARIANT_ARG_DECLARE) {
    VARIANT_ARGPTRS;

    int argc = 0;

    for (const Variant * variant : argptr) {
        if (variant->get_type() == VariantType::NIL) {
            break;
        }
        argc++;
    }

    return push_callable(p_callable, argptr, argc);
}

void MessageQueue::statistics() const {
    HashMap<Callable, int> call_count;
    int func_count = 0;
    int null_count = 0;

    uint32_t read_pos = 0;
    while (read_pos < buffer_end) {
        Message *message = (Message *)&buffer[read_pos];

        Object *target = object_for_entity(message->callable.get_object_id());

        if (target != nullptr) {

            switch (message->type & FLAG_MASK) {
                case TYPE_CALL: {
                    call_count[message->callable]++;
                } break;
            }

        } else {
            //object was deleted
            print_line("Object was deleted while awaiting a callback");

            null_count++;
        }

        read_pos += sizeof(Message);
        read_pos += sizeof(Variant) * message->args;
    }

    print_line("TOTAL BYTES: " + itos(buffer_end));
    print_line("NULL count: " + itos(null_count+null_object_calls.get()));
    print_line("FUNC count: " + itos(func_count));

    for (const eastl::pair<const Callable,int> &E : call_count) {
        print_line("CALL " + String(E.first) + ": " + ::to_string(E.second));
    }
}

int MessageQueue::get_max_buffer_usage() const {

    return buffer_max_used;
}

void MessageQueue::_call_function(const Callable& p_callable, const Variant* p_args, int p_argcount, bool p_show_error) {
    const Variant** argptrs = nullptr;
    if (p_argcount) {
        argptrs = (const Variant**)alloca(sizeof(Variant*) * p_argcount);
        for (int i = 0; i < p_argcount; i++) {
            argptrs[i] = &p_args[i];
        }
    }

    Callable::CallError ce;
    Variant ret;
    p_callable.call(argptrs, p_argcount, ret, ce);
    if (p_show_error && ce.error != Callable::CallError::CALL_OK) {
        ERR_PRINT("Error calling deferred method: " + Variant::get_callable_error_text(p_callable, argptrs, p_argcount, ce) + ".");
    }
}

void MessageQueue::flush()
{
    if (buffer_end > buffer_max_used)
    {
        buffer_max_used = buffer_end;
    }

    uint32_t read_pos = 0;

    //using reverse locking strategy
    _THREAD_SAFE_LOCK_

    if (flushing)
    {
        _THREAD_SAFE_UNLOCK_
        ERR_FAIL_COND(flushing); //already flushing, you did something odd
    }
    flushing = true;

    while (read_pos < buffer_end)
    {
        //lock on each iteration, so a call can re-add itself to the message queue
        TRACE_FREE_N(&buffer[read_pos],"MessageQueueAlloc");

        Message *message = (Message*)&buffer[read_pos];

        uint32_t advance = sizeof(Message);
        advance += sizeof(Variant) * message->args;

        //pre-advance so this function is reentrant
        read_pos += advance;

        _THREAD_SAFE_UNLOCK_

        Object *target = message->callable.get_object();

        if (target != nullptr)
        {
            switch (message->type & FLAG_MASK)
            {
            case TYPE_CALL:
                {
                    Variant *args = (Variant*)(message + 1);

                    // messages don't expect a return value

                    _call_function(message->callable, args, message->args, message->type & FLAG_SHOW_ERROR);
                }
                break;
            }
        }

        else {
            null_object_calls.increment();
        }
        Variant *args = (Variant*)(message + 1);
        for (int i = 0; i < message->args; i++)
        {
            args[i].~Variant();
        }

        message->~Message();

        _THREAD_SAFE_LOCK_
    }

    buffer_end = 0; // reset buffer
    flushing = false;
    _THREAD_SAFE_UNLOCK_
}

bool MessageQueue::is_flushing() const {

    return flushing;
}

MessageQueue::MessageQueue() {
    ERR_FAIL_COND_MSG(singleton != nullptr, "A MessageQueue singleton already exists.");
    singleton = this;
    StringName prop_name("memory/limits/message_queue/max_size_kb");
    buffer_size = GLOBAL_DEF_T_RST(prop_name, DEFAULT_QUEUE_SIZE_KB,uint32_t);
    ProjectSettings::get_singleton()->set_custom_property_info(
            prop_name, PropertyInfo(VariantType::INT, "memory/limits/message_queue/max_size_kb", PropertyHint::Range,
                               "1024,4096,1,or_greater"));
    buffer_size *= 1024;
    buffer = memnew_arr(uint8_t, buffer_size);
}

MessageQueue::~MessageQueue() {

    uint32_t read_pos = 0;

    while (read_pos < buffer_end) {

        Message *message = (Message *)&buffer[read_pos];
        Variant *args = (Variant *)(message + 1);
        int argc = message->args;
        for (int i = 0; i < argc; i++)
            args[i].~Variant();
        message->~Message();
        TRACE_FREE_N(message,"MessageQueueAlloc");

        read_pos += sizeof(Message);
        read_pos += sizeof(Variant) * message->args;
    }

    singleton = nullptr;
    memdelete_arr(buffer);
}
