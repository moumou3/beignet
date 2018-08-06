/*
 * Copyright © 2012 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "cl_base_object.h"

static pid_t invalid_thread_id = -1;

LOCAL void
cl_object_init_base(cl_base_object obj, cl_ulong magic)
{
  obj->magic = magic;
  obj->ref = 1;
  SET_ICD(obj->dispatch);
  LOCK_INIT(&obj->mutex, NULL);
#ifdef USE_COND
  pthread_cond_init(&obj->cond, NULL);
#endif
  obj->owner = invalid_thread_id;
  list_node_init(&obj->node);
}

LOCAL void
cl_object_destroy_base(cl_base_object obj)
{
  int ref = CL_OBJECT_GET_REF(obj);
  if (ref != 0) {
    DEBUGP(DL_ERROR, "CL object %p, call destroy with a reference %d", obj,
           ref);
    ASSERT(0);
  }

  if (!CL_OBJECT_IS_VALID(obj)) {
    DEBUGP(DL_ERROR,
           "CL object %p, call destroy while it is already a dead object", obj);
    ASSERT(0);
  }

  if (obj->owner != invalid_thread_id) {
    DEBUGP(DL_ERROR, "CL object %p, call destroy while still has a owener %d",
           obj, (int)obj->owner);
    ASSERT(0);
  }

  if (!list_node_out_of_list(&obj->node)) {
    DEBUGP(DL_ERROR, "CL object %p, call destroy while still belong to some object %p",
           obj, obj->node.p);
    ASSERT(0);
  }

  obj->magic = CL_OBJECT_INVALID_MAGIC;
  mutex_destroy(&obj->mutex);
  //pthread_cond_destroy(&obj->cond);
}

LOCAL cl_int
cl_object_take_ownership(cl_base_object obj, cl_int wait, cl_bool withlock)
{
  pid_t self;

  ASSERT(CL_OBJECT_IS_VALID(obj));

  self = GETTID();

  if (withlock == CL_FALSE)
    mutex_lock(&obj->mutex);

  if (obj->owner == self) { // Already get
    if (withlock == CL_FALSE)
      mutex_unlock(&obj->mutex);
    return 1;
  }

  if (obj->owner == invalid_thread_id) {
    obj->owner = self;

    if (withlock == CL_FALSE)
      mutex_unlock(&obj->mutex);
    return 1;
  }

  if (wait == 0) {
    if (withlock == CL_FALSE)
      mutex_unlock(&obj->mutex);
    return 0;
  }

  while (obj->owner != invalid_thread_id) {
#ifdef USE_COND 
    pthread_cond_wait(&obj->cond, &obj->mutex); 
#endif
  }

  obj->owner = self;

  if (withlock == CL_FALSE)
    mutex_unlock(&obj->mutex);

  return 1;
}

LOCAL void
cl_object_release_ownership(cl_base_object obj, cl_bool withlock)
{
  ASSERT(CL_OBJECT_IS_VALID(obj));

  if (withlock == CL_FALSE)
    mutex_lock(&obj->mutex);

  ASSERT(GETTID() == obj->owner);
  obj->owner = invalid_thread_id;
#ifdef USE_COND
  pthread_cond_broadcast(&obj->cond); 
#endif 

  if (withlock == CL_FALSE)
    mutex_unlock(&obj->mutex);
}

LOCAL void
cl_object_wait_on_cond(cl_base_object obj)
{
#ifdef USE_COND
  ASSERT(CL_OBJECT_IS_VALID(obj));
  pthread_cond_wait(&obj->cond, &obj->mutex);
#endif
}

LOCAL void
cl_object_notify_cond(cl_base_object obj)
{
#ifdef USE_COND
  ASSERT(CL_OBJECT_IS_VALID(obj));
  pthread_cond_broadcast(&obj->cond);
#endif
}
