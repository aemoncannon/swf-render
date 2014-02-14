// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// In the header you should put also a copyright notice something like:
//
// Copyright Aemon Cannon 2013,2013

#include <ruby.h>
#include <stdlib.h>

#include "flash_rasterizer.h"
#include "utils.h"

// Allocate two VALUE variables to hold the modules we'll create. Ruby values
// are all of type VALUE. Qnil is the C representation of Ruby's nil.
extern "C" VALUE SWFRender = Qnil;
extern "C" void Init_swf_render();
extern "C" VALUE method_render(
  VALUE self,
  VALUE swf_name,
  VALUE class_name,
  VALUE width,
  VALUE height,
  VALUE padding);

extern "C" VALUE method_render_spec(
  VALUE self,
  VALUE swf_name,
  VALUE class_name,
  VALUE spec,
  VALUE width,
  VALUE height,
  VALUE padding);


extern "C" VALUE ResultClass = Qnil;

static void Result_free(void *s) {
  xfree(s);
}
static VALUE Result_get_size(VALUE r) {
  struct Result *result;
  Data_Get_Struct(r, struct Result, result);
  return INT2NUM(result->size);
}
static VALUE Result_get_origin_x(VALUE r) {
  struct Result *result;
  Data_Get_Struct(r, struct Result, result);
  return INT2NUM(result->origin_x);
}
static VALUE Result_get_origin_y(VALUE r) {
  struct Result *result;
  Data_Get_Struct(r, struct Result, result);
  return INT2NUM(result->origin_y);
}
static VALUE Result_get_data(VALUE r) {
  struct Result *result;
  Data_Get_Struct(r, struct Result, result);
  return rb_str_new((char*)result->data, result->size);
}

// Initial setup function, takes no arguments and returns nothing. Some API
// notes:
// 
// * rb_define_module() creates and returns a top-level module by name
// 
// * rb_define_module_under() takes a module and a name, and creates a new
//   module within the given one
// 
// * rb_define_singleton_method() take a module, the method name, a reference to
//   a C function, and the method's arity, and exposes the C function as a
//   single method on the given module
// 
void Init_swf_render() {

  SWFRender = rb_define_module("SWFRender");
  rb_define_singleton_method(SWFRender, "render", (VALUE(*)(...))method_render, 5);
  rb_define_singleton_method(SWFRender, "render_spec", (VALUE(*)(...))method_render_spec, 6);

  ResultClass = rb_define_class_under(SWFRender, "Result", rb_cObject);
  rb_define_method(ResultClass, "get_size", (VALUE(*)(...))Result_get_size, 0);
  rb_define_method(ResultClass, "get_origin_x", (VALUE(*)(...))Result_get_origin_x, 0);
  rb_define_method(ResultClass, "get_origin_y", (VALUE(*)(...))Result_get_origin_y, 0);
  rb_define_method(ResultClass, "get_data", (VALUE(*)(...))Result_get_data, 0);
}

// The business logic -- this is the function we're exposing to Ruby. It returns
// a Ruby VALUE, and takes three VALUE arguments: the receiver object, and the
// method parameters. Notes on APIs used here:
// 
// * RARRAY_LEN(VALUE) returns the length of a Ruby array object
// * rb_ary_new2(int) creates a new Ruby array with the given length
// * rb_ary_entry(VALUE, int) returns the nth element of a Ruby array
// * NUM2INT converts a Ruby Fixnum object to a C int
// * INT2NUM converts a C int to a Ruby Fixnum object
// * rb_ary_store(VALUE, int, VALUE) sets the nth element of a Ruby array
//
VALUE method_render(
    VALUE self,
    VALUE swf_name,
    VALUE class_name,
    VALUE width,
    VALUE height,
    VALUE padding) {
  struct Result* result;
  result = ALLOC(struct Result);
  RunConfig config;
  config.input_swf = RSTRING_PTR(swf_name);
  config.class_name = RSTRING_PTR(class_name);
  config.width = NUM2INT(width);
  config.height = NUM2INT(height);
  config.padding = NUM2INT(padding);
  render_to_png_buffer(config, result);
  return Data_Wrap_Struct(ResultClass, NULL, Result_free, result);
}

VALUE method_render_spec(
    VALUE self,
    VALUE swf_name,
    VALUE class_name,
    VALUE spec,
    VALUE width,
    VALUE height,
    VALUE padding) {
  struct Result* result;
  result = ALLOC(struct Result);
  RunConfig config;
  config.input_swf = RSTRING_PTR(swf_name);
  config.class_name = RSTRING_PTR(class_name);
  config.spec = RSTRING_PTR(spec);
  config.width = NUM2INT(width);
  config.height = NUM2INT(height);
  config.padding = NUM2INT(padding);
  render_to_png_buffer(config, result);
  return Data_Wrap_Struct(ResultClass, NULL, Result_free, result);
}

