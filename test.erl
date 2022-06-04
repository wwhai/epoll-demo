f().
ConcatBin = fun(A, B) -> <<A/binary, B/binary>> end.
Bin = ConcatBin(<<"SSP",1, 0, 3>>, erlang:list_to_binary(lists:duplicate(1600,65))).
lists:foreach(fun(I)->
io:format("Send:~p\r\n", [I]),
{ok, Sock} = gen_tcp:connect("127.0.0.1", 2889, [binary, {packet, 0}]),
ok = gen_tcp:send(Sock, Bin),
ok = gen_tcp:send(Sock, <<"SSP",0>>),
ok = gen_tcp:close(Sock)
end, lists:seq(1,1)).

-- ping

f().
{ok, Sock} = gen_tcp:connect("127.0.0.1", 2889, [binary, {packet, 0}]),
ok = gen_tcp:send(Sock, <<"SSP",0:8>>),
ok = gen_tcp:close(Sock).

-- connect

f(), {ok, Sock} = gen_tcp:connect("127.0.0.1", 2889, [binary, {packet, 0}]),ok = gen_tcp:send(Sock, <<"SSP",2:8,32:16,"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX","SSP",02:8,32:16,"VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVAAAAA">>),ok = gen_tcp:close(Sock).

-- disconn
f().
{ok, Sock} = gen_tcp:connect("127.0.0.1", 2889, [binary, {packet, 0}]),
ok = gen_tcp:send(Sock, <<"SSP",4:8>>),
ok = gen_tcp:close(Sock).

-- send
f(),{ok, Sock} = gen_tcp:connect("127.0.0.1", 2889, [binary, {packet, 0}]),ok = gen_tcp:send(Sock, <<"SSP",6:8,5:16,"HELLO">>),ok = gen_tcp:close(Sock).

-- publish
f().
{ok, Sock} = gen_tcp:connect("127.0.0.1", 2889, [binary, {packet, 0}]),
ok = gen_tcp:send(Sock, <<"SSP",8:8,37:16,"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX","HELLO">>),
ok = gen_tcp:close(Sock).
