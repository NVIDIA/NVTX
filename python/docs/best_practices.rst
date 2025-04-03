Best practices
==============

Give, don't take
----------------

NVTX is primarily a one-way API. Your program gives information to the tool,
but it does not get actionable information back from the tool.
Some NVTX functions return values, but these should only be used as inputs to other NVTX functions.
Programs should behave exactly the same regardless of whether a tool is present or not.

Isolate NVTX annotations in a library using a domain
----------------------------------------------------

Programs may use multiple libraries that produce NVTX annotations.
A library should isolate its annotations by creating them within a dedicated domain.
Tools can group annotation data by library,
and provide options for which domains to enable or disable during the program execution.

Use categories to organize annotations
--------------------------------------

While domains are intended to separate the annotations from different libraries,
it may be useful to have separate categories for annotations within a library.
Tools are encouraged to logically group annotations into categories.
Using slashes in category names like filesystem paths allows the user to
create a hierarchy of categories, and tools should handle these as a hierarchy.

Reduce cache lookups
--------------------

NVTX is designed to produce minimal overhead during the program execution.
As such, it caches :class:`nvtx.Domain` and :class:`nvtx._lib.lib.EventAttributes` objects,
as well as :class:`nvtx._lib.lib.RegisteredString` objects and category IDs.

The functions :func:`nvtx.mark`, :func:`nvtx.push_range`, :func:`nvtx.pop_range`,
:func:`nvtx.start_range`, and :func:`nvtx.end_range` are convenient to use,
but they include cache lookups for the domain and event attributes.
Therefore, for best performance, it's better to use the methods from :class:`nvtx.Domain` instead.
For example:
::

   import nvtx

    def my_func(param: int):
        # This call includes a cache lookup for the domain, the message registered string,
        # the category ID and the event attributes object.
        # See `my_func_fast` for a faster alternative.
        nvtx.mark(message='my_func', domain='My Lib', category='my_category', payload=param)

        # continue with the function logic

   # Save a reference to the domain object,
   # so it can be accessed everywhere in the library code,
   # to avoid multiple calls to nvtx.get_domain()
   domain = nvtx.get_domain('My Lib')

   # Reuse category IDs and EventAttributes objects when possible
   # to avoid multiple calls to nvtx.get_category_id() and nvtx.get_event_attributes()
   category_id = domain.get_category_id('my_category')
   attr = domain.get_event_attributes(message='my_func')

   def my_func_fast(param: int):
       """Faster version of my_func() using the domain object directly."""
       attr.payload = param
       domain.mark(attr)    # No cache lookups

       # continue with the function logic
