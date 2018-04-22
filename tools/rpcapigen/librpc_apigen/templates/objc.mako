<%! import librpc %>
/*
 * THIS IS AN AUTOMATICALLY GENERATED FILE - DO NOT EDIT IT
 */

% for s in structs:
@interface ${s.name}
% for m in s.members:

% endfor
@end
% endof

% for i in interfaces:
@interface ${i.name}
- (instancetype) initWithClient:(RPCClient *)client andPath:(NSString *)path;
% for m in i.members:
% if type(m) is librpc.Method:
- (id)${m.name}${"".join(":(id){0}".format(a.name) for a in m.arguments)};
% endif
% endfor
@end

% endfor