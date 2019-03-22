/* -*- C++ -*- */

#include "flecstan-utility.h"
#include "flecstan-visitor.h"

namespace flecstan {

/*
-----------------------------------------------------------------
   Class for YAML                        Where processed below
-----------------------------------------------------------------
   flecsi_register_program               VisitVarDecl()
   flecsi_register_top_level_driver      VisitVarDecl()
-----------------------------------------------------------------
   flecsi_register_global_object         VisitCallExpr()
   flecsi_set_global_object              VisitCallExpr()
   flecsi_initialize_global_object       VisitCallExpr()
   flecsi_get_global_object              VisitCallExpr()
-----------------------------------------------------------------
   flecsi_register_task_simple           VisitVarDecl()
   flecsi_register_task                  VisitVarDecl()
   flecsi_register_mpi_task_simple       VisitVarDecl()
   flecsi_register_mpi_task              VisitVarDecl()
-----------------------------------------------------------------
   flecsi_color                          VisitCallExpr()
   flecsi_colors                         VisitCallExpr()
-----------------------------------------------------------------
   flecsi_execute_task_simple            VisitCallExpr()
   flecsi_execute_task                   VisitCallExpr()
   flecsi_execute_mpi_task_simple        VisitCallExpr()
   flecsi_execute_mpi_task               VisitCallExpr()
   flecsi_execute_reduction_task         VisitCallExpr()
-----------------------------------------------------------------
   flecsi_register_reduction_operation   VisitVarDecl()
-----------------------------------------------------------------
   flecsi_register_function              VisitVarDecl()
   flecsi_execute_function               VisitCallExpr()
   flecsi_function_handle                VisitCallExpr()
   flecsi_define_function_type           VisitTypeAliasDecl()
-----------------------------------------------------------------
   flecsi_register_data_client           VisitVarDecl()
   flecsi_register_field                 VisitVarDecl()
   flecsi_register_global                VisitVarDecl()
   flecsi_register_color                 VisitVarDecl()
-----------------------------------------------------------------
   flecsi_get_handle                     VisitCallExpr()
   flecsi_get_client_handle              VisitCallExpr()
   flecsi_get_handles                    VisitCallExpr()
   flecsi_get_handles_all                VisitCallExpr()
-----------------------------------------------------------------
   flecsi_get_global                     VisitCallExpr()
   flecsi_get_color                      VisitCallExpr()
   flecsi_get_mutator                    VisitCallExpr()
-----------------------------------------------------------------
*/



// -----------------------------------------------------------------------------
// Helper functions
// -----------------------------------------------------------------------------

// match
static bool match(
   const clang::CallExpr *&call, // output
   const clang::Expr *const expr,
   const MacroInvocation &m,
   const std::string &macro_want,
   const std::string &theclass = "",
   const std::string &thefunction = "",
   int min = -1,
   int max = -1
) {
   call = nullptr;
   if (m.name != macro_want)
      return false;

   if (theclass == "" && thefunction == "")
      return true;

   if (min == -1 && max == -1) {
      min = std::numeric_limits<int>::min();
      max = std::numeric_limits<int>::max();
   } else if (max == -1)
      max = min;

   call = getClassCall(expr, theclass, thefunction, min, max);
   if (call)
      report(
         "Link",
         "Function call: " + theclass + "::" + thefunction + "\n"
         "Matches macro: " + m.name + " (" + m.flc() + ")"
      );
   return call;
}



// strarg
static std::string strarg(
   const clang::CallExpr *const call,
   const std::size_t n
) {
   if (!call) return intwarn("strarg(): call == nullptr."), "";
   auto expr = call->getArg(n);
   if (!expr) return intwarn("strarg(): expr == nullptr."), "";
   auto bind = clang::dyn_cast<clang::CXXBindTemporaryExpr>(expr);
   if (!bind) return intwarn("strarg(): bind == nullptr."), "";
   auto conx = clang::dyn_cast<clang::CXXConstructExpr    >(bind->getSubExpr());
   if (!conx) return intwarn("strarg(): conx == nullptr."), "";
   auto cast = clang::dyn_cast<clang::ImplicitCastExpr    >(conx->getArg(0));
   if (!cast) return intwarn("strarg(): cast == nullptr."), "";
   auto strx = clang::dyn_cast<clang::StringLiteral       >(cast->getSubExpr());
   if (!strx) return intwarn("strarg(): strx == nullptr."), "";

   return strx->getString().str();
}



// nscontext
static void nscontext(
   const clang::Decl *const decl,
   std::vector<std::string> &ctx
) {
   for (const clang::DeclContext *dc = decl->getDeclContext();
        dc;  dc = dc->getParent()
   ) {
      // namespace?
      auto ns = clang::dyn_cast<clang::NamespaceDecl>(dc);
      if (ns)
         ctx.push_back(ns->getNameAsString());
   }
}



// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Visit*
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// For these functions, the bool return value tells Clang's AST visitor (the
// thing that's calling these functions) whether or not it should continue the
// AST traversal. For a typical code analysis tool, we'd anticipate that just
// returning true, always, is probably the right thing to do, absent some sort
// of terminal error.



// -----------------------------------------------------------------------------
// VisitVarDecl
// -----------------------------------------------------------------------------

// flecsi_register_program
// flecsi_register_top_level_driver
// flecsi_register_task_simple
// flecsi_register_task
// flecsi_register_mpi_task_simple
// flecsi_register_mpi_task
// flecsi_register_reduction_operation
// flecsi_register_function
// flecsi_register_data_client
// flecsi_register_field
// flecsi_register_global
// flecsi_register_color

bool Visitor::VisitVarDecl(const clang::VarDecl *const var)
{
   debug("Visitor::VisitVarDecl()");
   flecstan_debug(var->getNameAsString());

   // associated macro?
   const MacroInvocation *const iptr = prep.invocation(var->getLocStart());
   if (!iptr) return true;
   const MacroInvocation &m = *iptr;

   // for use below
   const clang::Expr *const expr = var->getInit();
   std::size_t pos = 0;
   const clang::CallExpr *call; // tbd

   // context
   std::vector<std::string> ctx;
   nscontext(var,ctx);



   // ------------------------
   // Top-level driver
   // interface
   // ------------------------

   // flecsi_register_program(program)
   if (match(call, expr, m, "flecsi_register_program")) {
      flecsi_register_program c(m);
      c.program = m.str(sema,pos++);
      yaml.push(c,ctx);
   }

   // flecsi_register_top_level_driver(driver)
   if (match(call, expr, m, "flecsi_register_top_level_driver")) {
      flecsi_register_top_level_driver c(m);
      c.driver = m.str(sema,pos++);
      yaml.push(c,ctx);
   }



   // ------------------------
   // Task registration
   // interface
   // ------------------------

   // flecsi_register_task_simple(task, processor, launch)
   if (match(call, expr, m, "flecsi_register_task_simple",
      "flecsi::execution::task_interface_u", "register_task", 3)) {
      flecsi_register_task_simple c(m);
      c.task      = m.str(sema,pos++);
      c.processor = m.str(sema,pos++);
      c.launch    = m.str(sema,pos++);
      c.hash      = strarg(call,2);
      yaml.push(c,ctx);
   }

   // flecsi_register_task(task, nspace, processor, launch)
   if (match(call, expr, m, "flecsi_register_task",
      "flecsi::execution::task_interface_u", "register_task", 3)) {
      flecsi_register_task c(m);
      c.task      = m.str(sema,pos++);
      c.nspace    = m.str(sema,pos++);
      c.processor = m.str(sema,pos++);
      c.launch    = m.str(sema,pos++);
      c.hash      = strarg(call,2);
      yaml.push(c,ctx);
   }

   // flecsi_register_mpi_task_simple(task)
   if (match(call, expr, m, "flecsi_register_mpi_task_simple",
      "flecsi::execution::task_interface_u", "register_task", 3)) {
      flecsi_register_mpi_task_simple c(m);
      c.task = m.str(sema,pos++);
      c.hash = strarg(call,2);
      yaml.push(c,ctx);
   }

   // flecsi_register_mpi_task(task, nspace)
   if (match(call, expr, m, "flecsi_register_mpi_task",
      "flecsi::execution::task_interface_u", "register_task", 3)) {
      flecsi_register_mpi_task c(m);
      c.task   = m.str(sema,pos++);
      c.nspace = m.str(sema,pos++);
      c.hash   = strarg(call,2);
      yaml.push(c,ctx);
   }



   // ------------------------
   // Reduction
   // interface
   // ------------------------

   // flecsi_register_reduction_operation(name, operation_type)
   if (match(call, expr, m, "flecsi_register_reduction_operation",
      "flecsi::execution::task_interface_u",
      "register_reduction_operation", 0)) {
      flecsi_register_reduction_operation c(m);
      c.name           = m.str(sema,pos++);
      c.operation_type = m.str(sema,pos++);
      yaml.push(c,ctx);
   }



   // ------------------------
   // Function
   // interface
   // ------------------------

   // flecsi_register_function(func, nspace)
   if (match(call, expr, m, "flecsi_register_function",
      "flecsi::execution::function_interface_u", "register_function", 0)) {
      flecsi_register_function c(m);
      c.func   = m.str(sema,pos++);
      c.nspace = m.str(sema,pos++);
      m.index = yaml.flecsi_register_function.matched.size();
      c.hash = m.hash;
      yaml.push(c,ctx);
   }

   // flecsi_execute_function(handle, ...)
   // See VisitCallExpr()

   // flecsi_function_handle(func, nspace)
   // See VisitCallExpr()

   // flecsi_define_function_type(func, return_type, ...)
   // See VisitTypeAliasDecl()



   // ------------------------
   // Registration
   // ------------------------

   // flecsi_register_data_client(client_type, nspace, name)
   if (match(call, expr, m, "flecsi_register_data_client",
      "flecsi::data::data_client_interface_u", "register_data_client", 1)) {
      const clang::TemplateArgumentList *const ta = getTemplateArgs(call);
      const clang::QualType qt = getTypeArg(ta,0);
      const clang::CXXRecordDecl *const cd = getClassDecl(qt);
      flecsi_register_data_client c(m);
      c.client_type   = m.str(sema,pos++);
      c.data_client_t = cd && isDerivedFrom(cd, "flecsi::data::data_client_t");
      c.nspace        = m.str(sema,pos++);
      c.name          = m.str(sema,pos++);
      yaml.push(c,ctx);
   }

   // flecsi_register_field(client_type, nspace, name, data_type,
   //                       storage_class, versions, ...)
   if (match(call, expr, m, "flecsi_register_field",
      "flecsi::data::field_interface_u", "register_field", 1)) {
      const clang::TemplateArgumentList *const ta = getTemplateArgs(call);
      flecsi_register_field c(m);
      c.client_type   = m.str(sema,pos++);
      c.nspace        = m.str(sema,pos++);
      c.name          = m.str(sema,pos++);
      c.data_type     = m.str(sema,pos++);
      c.storage_class = m.str(sema,pos++);
      c.versions      = getUIntArg(ta,5); // 5 = template argument position
      c.index_space   = getUIntArg(ta,6);
      yaml.push(c,ctx);
   }

   // flecsi_register_global(nspace, name, data_type, versions, ...)
   if (match(call, expr, m, "flecsi_register_global",
      "flecsi::data::field_interface_u", "register_field", 1)) {
      const clang::TemplateArgumentList *const ta = getTemplateArgs(call);
      flecsi_register_global c(m);
      c.nspace    = m.str(sema,pos++);
      c.name      = m.str(sema,pos++);
      c.data_type = m.str(sema,pos++);
      c.versions  = getUIntArg(ta,5);
      yaml.push(c,ctx);
   }

   // flecsi_register_color(nspace, name, data_type, versions, ...)
   if (match(call, expr, m, "flecsi_register_color",
      "flecsi::data::field_interface_u", "register_field", 1)) {
      const clang::TemplateArgumentList *const ta = getTemplateArgs(call);
      flecsi_register_color c(m);
      c.nspace    = m.str(sema,pos++);
      c.name      = m.str(sema,pos++);
      c.data_type = m.str(sema,pos++);
      c.versions  = getUIntArg(ta,5);
      yaml.push(c,ctx);
   }

   return true;

} // VisitVarDecl



// -----------------------------------------------------------------------------
// VisitCallExpr
// -----------------------------------------------------------------------------

// flecsi_register_global_object
// flecsi_set_global_object
// flecsi_initialize_global_object
// flecsi_get_global_object
// flecsi_color
// flecsi_colors
// flecsi_execute_task_simple
// flecsi_execute_task
// flecsi_execute_mpi_task_simple
// flecsi_execute_mpi_task
// flecsi_execute_reduction_task
// flecsi_execute_function
// flecsi_function_handle
// flecsi_get_handle
// flecsi_get_client_handle
// flecsi_get_handles
// flecsi_get_handles_all
// flecsi_get_global
// flecsi_get_color
// flecsi_get_mutator

bool Visitor::VisitCallExpr(const clang::CallExpr *const expr)
{
   debug("Visitor::VisitCallExpr()");

   // associated macro?
   const MacroInvocation *const iptr = prep.invocation(expr->getLocStart());
   if (!iptr) return true;
   const MacroInvocation &m = *iptr;

   // for use below
   std::size_t pos = 0;
   const clang::CallExpr *call; // tbd

   // context
   // Doesn't work at the moment, for CallExpr
   // std::vector<std::string> ctx;
   // nscontext(expr,ctx);



   // ------------------------
   // Object registration
   // interface
   // ------------------------

   // flecsi_register_global_object(index, nspace, type)
   if (match(call, expr, m, "flecsi_register_global_object",
      "flecsi::execution::context_u", "register_global_object")) {
      flecsi_register_global_object c(m);
      c.index  = m.str(sema,pos++);
      c.nspace = m.str(sema,pos++);
      c.type   = m.str(sema,pos++);
      yaml.push(c);
   }

   // flecsi_set_global_object(index, nspace, type, obj)
   if (match(call, expr, m, "flecsi_set_global_object",
      "flecsi::execution::context_u", "set_global_object")) {
      flecsi_set_global_object c(m);
      c.index  = m.str(sema,pos++);
      c.nspace = m.str(sema,pos++);
      c.type   = m.str(sema,pos++);
      c.obj    = m.str(sema,pos++);
      yaml.push(c);
   }

   // flecsi_initialize_global_object(index, nspace, type, ...)
   if (match(call, expr, m, "flecsi_initialize_global_object",
      "flecsi::execution::context_u", "initialize_global_object")) {
      flecsi_initialize_global_object c(m);
      c.index  = m.str(sema,pos++);
      c.nspace = m.str(sema,pos++);
      c.type   = m.str(sema,pos++);
      getVarArgsFunction(expr, c.varargs, 1);
      yaml.push(c);
   }

   // flecsi_get_global_object(index, nspace, type)
   if (match(call, expr, m, "flecsi_get_global_object",
      "flecsi::execution::context_u", "get_global_object")) {
      flecsi_get_global_object c(m);
      c.index  = m.str(sema,pos++);
      c.nspace = m.str(sema,pos++);
      c.type   = m.str(sema,pos++);
      yaml.push(c);
   }



   // ------------------------
   // Task execution
   // interface: color
   // ------------------------

   // Remark: For flecsi_color[s], I specifically needed to have the last
   // two match() arguments, as opposed to having calls like this:
   //    match(call, expr, m, "flecsi_color")
   //    match(call, expr, m, "flecsi_colors")
   // That's because both of those macros, while simple, actually generate
   // two CallExprs in the AST: one to instance(), and the other to color()
   // or colors(). Without the additional check for the exact name of the
   // function being called in the "call expression" (CallExpr), each macro
   // invocation inadvertently generated two identical entries in the YAML.

   // flecsi_color
   if (match(call, expr, m, "flecsi_color",
      "flecsi::execution::context_u", "color")) {
      flecsi_color c(m);
      yaml.push(c);
   }

   // flecsi_colors
   if (match(call, expr, m, "flecsi_colors",
      "flecsi::execution::context_u", "colors")) {
      flecsi_colors c(m);
      yaml.push(c);
   }



   // ------------------------
   // Task execution
   // interface: execute
   // ------------------------

   // Remark: Each of the following has two lines that look like this:
   //    c.hash = m.hash;
   //    m.index = yaml.<macro name>.matched.size();
   // These help with the sort of goofy but apparently necessary communications
   // hack between the current function, and VisitCXXConstructExpr() later in
   // this file. The basic idea is that the strings FleCSI's task execution
   // macros use for hashing don't seem to be accessible here - they've already
   // been hashed into integers, via const_string_t::hash(). But, I was able to
   // get at those strings by writing VisitCXXConstructExpr() and looking for
   // const_string_t constructions in the context of the task execution macros.
   // If this function is called first, before VisitCXXConstructExpr(), then the
   // m.index computation is used by the latter. If VisitCXXConstructExpr() is
   // called first, it sets m.hash, which is extracted here. Either way, the
   // objects being pushed into the yaml structure, below, eventually get the
   // hash string. (Which we use later, in our code's analysis stage, in order
   // to compare against the hash strings used in task registrations.) See the
   // VisitCXXConstructExpr() function, below, for more information.

   // flecsi_execute_task_simple(task, launch, ...)
   if (match(call, expr, m, "flecsi_execute_task_simple",
      "flecsi::execution::task_interface_u", "execute_task")) {
      flecsi_execute_task_simple c(m);
      c.task   = m.str(sema,pos++);
      c.launch = m.str(sema,pos++);
      getVarArgsFunction(expr, c.varargs);
      m.index = yaml.flecsi_execute_task_simple.matched.size();
      c.hash = m.hash;
      yaml.push(c);
   }

   // flecsi_execute_task(task, nspace, launch, ...)
   if (match(call, expr, m, "flecsi_execute_task",
      "flecsi::execution::task_interface_u", "execute_task")) {
      flecsi_execute_task c(m);
      c.task   = m.str(sema,pos++);
      c.nspace = m.str(sema,pos++);
      c.launch = m.str(sema,pos++);
      getVarArgsFunction(expr, c.varargs);
      m.index = yaml.flecsi_execute_task.matched.size();
      c.hash = m.hash;
      yaml.push(c);
   }

   // flecsi_execute_mpi_task_simple(task, ...)
   if (match(call, expr, m, "flecsi_execute_mpi_task_simple",
      "flecsi::execution::task_interface_u", "execute_task")) {
      flecsi_execute_mpi_task_simple c(m);
      c.task = m.str(sema,pos++);
      getVarArgsFunction(expr, c.varargs);
      m.index = yaml.flecsi_execute_mpi_task_simple.matched.size();
      c.hash = m.hash;
      yaml.push(c);
   }

   // flecsi_execute_mpi_task(task, nspace, ...)
   if (match(call, expr, m, "flecsi_execute_mpi_task",
      "flecsi::execution::task_interface_u", "execute_task")) {
      flecsi_execute_mpi_task c(m);
      c.task   = m.str(sema,pos++);
      c.nspace = m.str(sema,pos++);
      getVarArgsFunction(expr, c.varargs);
      m.index = yaml.flecsi_execute_mpi_task.matched.size();
      c.hash = m.hash;
      yaml.push(c);
   }

   // flecsi_execute_reduction_task(task, nspace, launch, type, datatype, ...)
   if (match(call, expr, m, "flecsi_execute_reduction_task",
      "flecsi::execution::task_interface_u", "execute_task")) {
      flecsi_execute_reduction_task c(m);
      c.task     = m.str(sema,pos++);
      c.nspace   = m.str(sema,pos++);
      c.launch   = m.str(sema,pos++);
      c.type     = m.str(sema,pos++);
      c.datatype = m.str(sema,pos++);
      getVarArgsFunction(expr, c.varargs);
      // m.index = yaml.flecsi_execute_reduction_task.matched.size();
      // c.hash = m.hash;
      c.hash = c.nspace + "::" + c.task; // for now, easier than the hacky crap
      yaml.push(c);
   }



   // ------------------------
   // Function
   // interface
   // ------------------------

   // flecsi_register_function(func, nspace)
   // See VisitVarDecl()

   // flecsi_execute_function(handle, ...)
   if (match(call, expr, m, "flecsi_execute_function",
      "flecsi::execution::function_interface_u", "execute_function")) {
      flecsi_execute_function c(m);
      c.handle = m.str(sema,pos++);
      getVarArgsFunction(expr, c.varargs, 1);
      yaml.push(c);
   }

   // flecsi_function_handle(func, nspace)
   if (match(call, expr, m, "flecsi_function_handle",
      "flecsi::utils::const_string_t", "hash")) {
      flecsi_function_handle c(m);
      c.func   = m.str(sema,pos++);
      c.nspace = m.str(sema,pos++);
      m.index = yaml.flecsi_function_handle.matched.size();
      c.hash = m.hash;
      yaml.push(c);
   }

   // flecsi_define_function_type(func, return_type, ...)
   // See VisitTypeAliasDecl()



   // ------------------------
   // Handle-related
   // ------------------------

   // flecsi_get_handle
   if (match(call, expr, m, "flecsi_get_handle",
      "flecsi::data::field_interface_u", "get_handle", 1)) {
      const clang::TemplateArgumentList *const ta = getTemplateArgs(expr);
      flecsi_get_handle c(m);
      c.client_handle = m.str(sema,pos++);
      c.nspace        = m.str(sema,pos++);
      c.name          = m.str(sema,pos++);
      c.data_type     = m.str(sema,pos++);
      c.storage_class = m.str(sema,pos++);
      c.version       = getUIntArg(ta,5); // 5 = template argument position
      yaml.push(c);
   }

   // flecsi_get_client_handle
   if (match(call, expr, m, "flecsi_get_client_handle",
      "flecsi::data::data_client_interface_u", "get_client_handle")) {
      flecsi_get_client_handle c(m);
      c.client_type = m.str(sema,pos++);
      c.nspace      = m.str(sema,pos++);
      c.name        = m.str(sema,pos++);
      yaml.push(c);
   }

   // flecsi_get_handles
   if (match(call, expr, m, "flecsi_get_handles",
      "flecsi::data::field_interface_u", "get_handles", 2)) {
      flecsi_get_handles c(m);
      c.client        = m.str(sema,pos++);
      c.nspace        = m.str(sema,pos++);
      c.data_type     = m.str(sema,pos++);
      c.storage_class = m.str(sema,pos++);
      c.version       = m.str(sema,pos++);
      getVarArgsFunction(expr, c.varargs, 2);
      yaml.push(c);
   }

   // flecsi_get_handles_all
   if (match(call, expr, m, "flecsi_get_handles_all",
      "flecsi::data::field_interface_u", "get_handles", 2)) {
      flecsi_get_handles_all c(m);
      c.client        = m.str(sema,pos++);
      c.data_type     = m.str(sema,pos++);
      c.storage_class = m.str(sema,pos++);
      c.version       = m.str(sema,pos++);
      getVarArgsFunction(expr, c.varargs, 2);
      yaml.push(c);
   }



   // ------------------------
   // For some flecsi_get_*
   // macros
   // ------------------------

   // flecsi_get_global
   if (match(call, expr, m, "flecsi_get_global",
      "flecsi::data::field_interface_u", "get_handle", 1)) {
      const clang::TemplateArgumentList *const ta = getTemplateArgs(expr);
      flecsi_get_global c(m);
      c.nspace    = m.str(sema,pos++);
      c.name      = m.str(sema,pos++);
      c.data_type = m.str(sema,pos++);
      c.version   = getUIntArg(ta,5); // 5 = template argument position
      yaml.push(c);
   }

   // flecsi_get_color
   if (match(call, expr, m, "flecsi_get_color",
      "flecsi::data::field_interface_u", "get_handle", 1)) {
      const clang::TemplateArgumentList *const ta = getTemplateArgs(expr);
      flecsi_get_color c(m);
      c.nspace    = m.str(sema,pos++);
      c.name      = m.str(sema,pos++);
      c.data_type = m.str(sema,pos++);
      c.version   = getUIntArg(ta,5); // 5 = template argument position
      yaml.push(c);
   }

   // flecsi_get_mutator
   if (match(call, expr, m, "flecsi_get_mutator",
      "flecsi::data::field_interface_u", "get_mutator", 2)) {
      const clang::TemplateArgumentList *const ta = getTemplateArgs(expr);
      flecsi_get_mutator c(m);
      c.client_handle = m.str(sema,pos++);
      c.nspace        = m.str(sema,pos++);
      c.name          = m.str(sema,pos++);
      c.data_type     = m.str(sema,pos++);
      c.storage_class = m.str(sema,pos++);
      c.version       = getUIntArg(ta,5); // 5 = template argument position
      pos++;  // c.version pulled via other means; skip to c.slots...
      c.slots         = m.str(sema,pos++);
      yaml.push(c);
   }

   return true;

} // VisitCallExpr



// -----------------------------------------------------------------------------
// VisitTypeAliasDecl
// -----------------------------------------------------------------------------

// flecsi_define_function_type

bool Visitor::VisitTypeAliasDecl(const clang::TypeAliasDecl *const tad)
{
   debug("Visitor::VisitTypeAliasDecl()");
   flecstan_debug(tad->getNameAsString());

   // associated macro?
   const MacroInvocation *const iptr = prep.invocation(tad->getLocStart());
   if (!iptr) return true;
   const MacroInvocation &m = *iptr;

   // for use below
   std::size_t pos = 0;

   // context
   std::vector<std::string> ctx;
   nscontext(tad,ctx);



   // ------------------------
   // Function
   // interface
   // ------------------------

   // flecsi_register_function(func, nspace)
   // See VisitVarDecl()

   // flecsi_execute_function(handle, ...)
   // See VisitCallExpr()

   // flecsi_function_handle(func, nspace)
   // See VisitCallExpr()

   // flecsi_define_function_type(func, return_type, ...)
   if (m.name == "flecsi_define_function_type") {
      std::string thetype;

      flecsi_define_function_type c(m);
      c.func        = m.str(sema,pos++);
      c.return_type = m.str(sema,pos++);
      getVarArgsTemplate(tad, c.varargs, thetype);
      yaml.push(c,ctx);

      report(
         "Link",
         "Function type: " + tad->getNameAsString() + " = " + thetype + "\n"
         "Matches macro: " + m.name + " (" + m.flc() + ")"
      );
   }

   return true;

} // VisitTypeAliasDecl



// -----------------------------------------------------------------------------
// VisitCXXConstructExpr
// -----------------------------------------------------------------------------

bool Visitor::VisitCXXConstructExpr(const clang::CXXConstructExpr *const cons)
{
   debug("Visitor::VisitCXXConstructExpr()");

   // associated macro?
   const MacroInvocation *const iptr = prep.invocation(cons->getLocStart());
   if (!iptr) return true;
   const MacroInvocation &m = *iptr;

   // I believe that FleCSI's task execution macros are, at present, defined
   // in such a way that we'll get to this point (having passed the invocation
   // test above) only if the present CXXConstructExpr is for a flecsi::utils::
   // const_string_t construction, not any other type of construction. For
   // general safety and robustness, however, I'm including various tests below,
   // which should help to guarantee that we're actually dealing with a call
   // to const_string_t's constructor - not, somehow, with something else. This
   // way, we won't inadvertently extract some non-hash-string-related nonsense
   // and somehow think it's associated with a hash string.

   // Update, 2019-02-25. We're doing this process for flecsi_register_function
   // and flecsi_function_handle too.

   auto cxxt = clang::dyn_cast<clang::CXXTemporaryObjectExpr>(cons);
   if (!cxxt) return true;
   auto elab = clang::dyn_cast<clang::ElaboratedType>(cxxt->getType());
   if (!elab) return true;

   if (elab->getNamedType().getAsString() !=
      "class flecsi::utils::const_string_t")
      return true;

   // For extra safety.
   // This shouldn't happen; const_string_t's constructor takes one argument
   if (cxxt->getNumArgs() != 1) {
      intwarn("In VisitCXXConstructExpr, "
              "CXXTemporaryObjectExpr.getNumArgs() != 1.");
      return true;
   }

   // This shouldn't happen; const_string_t's constructor takes a string literal
   auto strlit = clang::dyn_cast<clang::StringLiteral>(cxxt->getArg(0));
   if (!strlit) {
      intwarn("In VisitCXXConstructExpr, "
              "CXXTemporaryObjectExpr.getArg(0) != StringLiteral.");
      return true;
   }

   // Update, 2019-02-26. It seems that sometime since I last updated FleCSI,
   // which had been quite some time, its task execution macros were updated.
   // In particular, the underlying execute_task<>() function template has been
   // updated, and now takes TWO hashes, not one, in its template argument list.
   // One for (task) as before, and another for a new thing called (operation),
   // given as 0 by the macros. (It may be an as-yet-unused placeholder for an
   // upcoming feature; I don't know.) I discovered all of this after I updated
   // FleCSI and suddenly saw false positive errors, reporting task executions
   // without registrations (the executions thinking that the "0" I spoke of
   // was the hash, the registrations seeing the function-name hash as always).
   // Here's a hack to deal with this situation. It really would be nice if code
   // analysis weren't so fragile with regards to tiny implementation details
   // of FleCSI, but prospects in this regard may be limited. :-/
   const std::string hash = strlit->getString().str();
   if (hash == "0") // <== I know, this is really ridiculous
      return true;

   // OK, the above tests all passed.
   // Now, we can extract the string that FleCSI hashes.
   // The result must be communicated to the structures that another Visit*()
   // function created (or will create) for the relevant FleCSI macro. See the
   // comments below for how this is accomplished...

   // If the present function (VisitCXXConstructExpr()) is called before
   // the other Visit*() function, then the other function can extract the
   // hash as communicated here (via the MacroInvocation that both functions
   // associate with), and thereby fully initialize, right off the bat, the
   // contents of the requisite data structure.
   m.hash = hash;

   // If the other Visit*() function is called before the present function
   // (VisitCXXConstructExpr()), then the present function can use the index
   // that was set up by the other one, and place the hash into the requisite
   // data structure (that was mostly initialized earlier, but missing the
   // hash), and thereby finalize that data structure.
   if (m.index != -1) {
      #define flecsi_finalize_hash(mac) \
         if (m.name == #mac) yaml.mac.matched[m.index].hash = m.hash

      flecsi_finalize_hash(flecsi_execute_task_simple);
      flecsi_finalize_hash(flecsi_execute_task);
      flecsi_finalize_hash(flecsi_execute_mpi_task_simple);
      flecsi_finalize_hash(flecsi_execute_mpi_task);
      // flecsi_finalize_hash(flecsi_execute_reduction_task); // did another way

      flecsi_finalize_hash(flecsi_register_function);
      flecsi_finalize_hash(flecsi_function_handle);

      #undef flecsi_finalize_hash
   }

   return true;

} // VisitCXXConstructExpr

} // namespace flecstan
