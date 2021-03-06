/*
 * Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2016, 2019 SAP SE. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/thread.hpp"

frame JavaThread::pd_last_frame() {
  assert(has_last_Java_frame(), "must have last_Java_sp() when suspended");

  intptr_t* sp = last_Java_sp();
  address pc = _anchor.last_Java_pc();

  // Last_Java_pc ist not set if we come here from compiled code.
  if (pc == NULL) {
    pc = (address) *(sp + 14);
  }

  return frame(sp, pc);
}

bool JavaThread::pd_get_top_frame_for_profiling(frame* fr_addr, void* ucontext, bool isInJava) {
  assert(this->is_Java_thread(), "must be JavaThread");

  // If we have a last_Java_frame, then we should use it even if
  // isInJava == true.  It should be more reliable than ucontext info.
  if (has_last_Java_frame() && frame_anchor()->walkable()) {
    *fr_addr = pd_last_frame();
    return true;
  }

  if (isInJava) {
    ucontext_t* uc = (ucontext_t*) ucontext;
    frame ret_frame((intptr_t*)uc->uc_mcontext.gregs[15/*Z_SP*/],
                   (address)uc->uc_mcontext.psw.addr);

    if (ret_frame.pc() == NULL) {
      // ucontext wasn't useful
      return false;
    }

    if (ret_frame.is_interpreted_frame()) {
      frame::z_ijava_state* istate = ret_frame.ijava_state_unchecked();
       if ((stack_base() >= (address)istate && (address)istate > stack_end()) ||
           MetaspaceObj::is_valid((Method*)(istate->method)) == false) {
         return false;
       }
       uint64_t reg_bcp = uc->uc_mcontext.gregs[13/*Z_BCP*/];
       uint64_t istate_bcp = istate->bcp;
       uint64_t code_start = (uint64_t)(((Method*)(istate->method))->code_base());
       uint64_t code_end = (uint64_t)(((Method*)istate->method)->code_base() + ((Method*)istate->method)->code_size());
       if (istate_bcp >= code_start && istate_bcp < code_end) {
         // we have a valid bcp, don't touch it, do nothing
       } else if (reg_bcp >= code_start && reg_bcp < code_end) {
         istate->bcp = reg_bcp;
       } else {
         return false;
       }
    }
    if (!ret_frame.safe_for_sender(this)) {
      // nothing else to try if the frame isn't good
      return false;
    }
    *fr_addr = ret_frame;
    return true;
  }
  // nothing else to try
  return false;
}

// Forte Analyzer AsyncGetCallTrace profiling support.
bool JavaThread::pd_get_top_frame_for_signal_handler(frame* fr_addr, void* ucontext, bool isInJava) {
  return pd_get_top_frame_for_profiling(fr_addr, ucontext, isInJava);
}

void JavaThread::cache_global_variables() { }
