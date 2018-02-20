<%inherit file="base.mako"/>
<%namespace name="partials" file="partials.mako"/>

<h1>Type definitions</h1>
% for t in typedefs:
    <div>
        ${partials.typedef(t)}
    </div>
% endfor
<h1>Data structures</h1>
% for s in structures:
    ${partials.struct(s)}
% endfor
<h1>Functions</h1>
