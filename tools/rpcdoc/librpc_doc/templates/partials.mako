<%! import librpc %>
<%def name="type(t)">
    %if t:
        %if isinstance(t, librpc.Type):
            <span class="type">
                <a href="type-${t.name}.html">${t.name}</a>
                ${generic_vars_type(t)}
            </span>
        %else:
            % if t.proxy:
                <span class="type">${t.proxy_variable}</span.
            %else:
                <span class="type">
                    %if t.type:
                        <a href="type-${t.type.name}.html">${t.type.name}</a>
                        ${generic_vars(t)}
                    %endif
                </span>
            %endif
        %endif
    %endif
</%def>

<%def name="struct(s)">
<h3 id="${s.name}">struct <span class="type">${s.name}</span></h3>
<p>${s.description}</p>
<h4>Members:</h4>
<ul>
    % for m in s.members:
    <li>
        ${type(m.type)} ${m.name}
        <p>
            ${m.description}
        </p>
    </li>
    % endfor
</ul>
</%def>

<%def name="union(u)">
<h3 id="${u.name}">union <span class="type">${u.name}</span></h3>
<p>${u.description}</p>
<h4>Branches:</h4>
<ul>
    % for m in u.members:
    <li>
        ${type(m.type)} ${m.name}
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
        <h3>type ${type(t)} = <span class="type">${t.definition.canonical | h}</span></h3>
    %endif
    <p>
        ${t.description}
    </p>
</%def>

<%def name="enum(e)">
<h3 id="${e.name}">enum <span class="type">${e.name}</span></h3>
<p>${e.description}</p>
<h4>Members:</h4>
<ul>
    % for m in e.members:
    <li>
        <span class="type">${m.name}</span>
        <p>
            ${m.description}
        </p>
    </li>
    % endfor
</ul>
</%def>

<%def name="generic_vars_type(t)">
    %if t:
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
    %endif
</%def>

<%def name="generic_vars(t)">
    %if t.type.generic:
        &lt;
        %for v in t.generic_variables:
            ${type(v)}
            %if not loop.last:
                ,
            %endif
        %endfor
        &gt;
    %endif
</%def>
