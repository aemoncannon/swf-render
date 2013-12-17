#include <ruby.h>
#include "flash_rasterizer.h"

// Allocate two VALUE variables to hold the modules we'll create. Ruby values
// are all of type VALUE. Qnil is the C representation of Ruby's nil.
extern "C" VALUE SWFRender = Qnil;

extern "C" void Init_swf_render();
extern "C" VALUE method_render(VALUE self, VALUE swf_name);

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
  rb_define_singleton_method(SWFRender, "render", (VALUE(*)(...))method_render, 1);
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
VALUE method_render(VALUE self, VALUE swf_name, VALUE out_name) {
  render(RSTRING_PTR(swf_name), RSTRING_PTR(out_name));
  return INT2NUM(0);
}
