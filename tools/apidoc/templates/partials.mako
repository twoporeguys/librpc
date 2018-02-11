<%def name="type(t)">
    %if t:
        <span class="type"><a href="type-${t.canonical}.html">${t.canonical | h}</a></span>
    %endif
</%def>

<%def name="struct(s)">
<h3 id="${s.name}">struct <span class="type">${s.name}</span>${generic_vars(s)}</h3>
<p>
    ${s.description}
</p>
<h4>Members:</h4>
<ul>
    % for m in s.members:
    <li>
        <span class="type">${m.type.canonical if m.type else "" | h}</span> ${m.name}
        <p>
            ${m.description}
        </p>
    </li>
    % endfor
</ul>
</%def>

<%def name="typedef(t)">
    %if t.is_builtin:
        <h3>builtin <span class="type">${t.name}</span></h3>
    %else:
        <h3>type <span class="type">${t.name}</span>${generic_vars(t)} = <span class="type">${t.definition.canonical | h}</span></h3>
    %endif
    <p>
        ${t.description}
    </p>
</%def>

<%def name="generic_vars(t)">
    %if t.generic:
        &lt;
        %for v in t.generic_variables:
            <span class="type">${v}</span>
            %if not loop.last:
                ,
            %endif
        %endfor
        &gt;
    %endif
</%def>
