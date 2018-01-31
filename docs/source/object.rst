Object model
============

Instances and interfaces
------------------------

There are five kinds of entities that are exposed by a librpc server:

- instances (objects)
- interfaces (implemented by objects)
- methods (implemented by interfaces)
- properties (implemented by interfaces)
- events (or signals, also implemented by interfaces)

Instances form a tree structure (called "object tree") that's a top-level
entry point for discovering server features. Each of the instances in the
tree might implement one or more interfaces. Each interface, might implement
one or more methods, properties or signals. Objects are identified by paths,
with the root of the object tree (called root node) having path ``/``.

That said, a call to a librpc server consists of four parts:

- object path
- interface name
- method name
- method arguments (optional)

The basic principles of librpc operation are very similar to
`D-Bus <https://www.freedesktop.org/>`_.

There are three builtin interfaces that are implemented by each and every
object on the server:

``com.twoporeguys.librpc.Discoverable``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This interface is reponsible for providing the user a full or partial view
on the object tree. It has the following members:

- ``get_instances()`` method - retrieves a list of child instances. When
  called on a root node, returns a list of all instances on the server.
- ``instance_added`` event - notifies the client about a new instance being
  added to the server
- ``instance_removed`` event - notifies the client about an instance being
  removed from the server

``com.twoporeguys.librcpc.Introspectable``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This interface provides object introspection features. It has the following
members:

- ``get_interfaces()`` method - retrieves a list of interfaces implemented
  by the object
- ``get_methods(interface)`` method - retrieves a list of methods
  implemented by the object
- ``interface_exists(interface)`` method - returns ``true`` if object
  implements given ``interface`` or ``false`` otherwise
- ``interface_added`` event - notifies the client that an interface was added
  to the object
- ``interface_removed`` event - notifies the client that object no longer
  implements an interface

``com.twoporeguys.librpc.Observable``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This interface provides access to object properties. It has the following
members:

- ``get(interface, property)`` method - returns a value of ``property`` or
  an ``interface``
- ``get_all(interface)`` method - returns a list of properties exposed by
  an ``interface`` along with their values
- ``set(interface, property, value)`` method - sets a value of the ``property``
  of an ``interface`` to ``value``
- ``changed`` signal - notifies the client that the value of a property
  has changed
