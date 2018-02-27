<%inherit file="base.mako"/>
<%namespace name="partials" file="partials.mako"/>

<h1>Interface ${iface.name}</h1>
<p>
    %if iface.description:
        ${iface.description}
    %else:
        <i>No description</i>
    %endif
</p>

<h3>Methods:</h3>
% for m in methods:
    <h4><a name="${m.name}" href="#${m.name}">${m.name}</a></h4>
    <p>${m.description}</p>
    <ul>
        % for arg in m.arguments:
            <li>
                <p>Type: ${partials.type(arg.type)}</p>
                <p>Description ${arg.description}</p>
            </li>
        % endfor
    </ul>
% endfor

<h3>Properties:</h3>
% for p in properties:
    <h5>${partials.type(p.type)} ${p.name}</h5>
% endfor

<h3>Events:</h3>
% for e in events:
    <h5>${e.name}</h5>
% endfor