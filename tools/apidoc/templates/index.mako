<%inherit file="base.mako"/>
<%namespace name="partials" file="partials.mako"/>

<h1>${name} API documentation</h1>
<h3>Types:</h3>
<ul>
    % for i in types:
        <li><a href="type-${i.name}.html">${i.name}${partials.generic_vars(i)}</a></li>
    % endfor
    % for i in typedefs:
        <li><a href="type-${i.name}.html">${i.name}${partials.generic_vars(i)}</a></li>
    % endfor
</ul>

<h3>Structures and unions:</h3>
<ul>
    % for i in structs:
        <li><a href="type-${i.name}.html">${i.name}</a></li>
    % endfor
</ul>

<h3>Interfaces:</h3>
<ul>
    % for i in interfaces:
        <li><a href="interface-${i.name}.html">${i.name}</a></li>
    % endfor
</ul>
