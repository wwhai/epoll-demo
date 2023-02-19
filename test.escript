#!/usr/bin/env escript
%% -*- erlang -*-
main([]) ->
    io:format("Help: escript test.escript [port]~n");

main([Args]) ->
    io:format("[LOG] Server :127.0.0.1:~p~n", [Args]),
    lists:foreach(fun(I) ->
        spawn(
            fun() ->
                io:format("[LOG] START TEST =====> ~p~n", [I]),
                {ok, Socket} = gen_tcp:connect("127.0.0.1", 2889, [binary, {packet, 0}, {active, false}]),
                send_conn(Socket),
                send_ping(Socket),
                send_send(Socket),
                send_publish(Socket),
                ok = gen_tcp:close(Socket)
            end
        )
    end, lists:seq(1, 100)),
    timer:sleep(3000).


send_ping(Socket) ->
    io:format("[LOG] SEND PING~n"),
    ok = gen_tcp:send(Socket, <<"SSP", 0:8>>),
    receive_byte(Socket).

send_conn(Socket) ->
    io:format("[LOG] SEND CONN~n"),
    CONN_PKT = <<"SSP", 2:8, 32:16,"A0000000000000000000000000000001">>,
    ok = gen_tcp:send(Socket, CONN_PKT),
    receive_byte(Socket).

send_send(Socket) ->
    io:format("[LOG] SEND SEND~n"),
    ok = gen_tcp:send(Socket, <<"SSP", 6:8, 5:16,"HELLO">>),
    receive_byte(Socket).

send_publish(Socket) ->
    io:format("[LOG] SEND PUBLISH~n"),
    PUBLISH_PKT = <<"SSP", 8:8, 37:16, "A0000000000000000000000000000002", "HELLO">>,
    ok = gen_tcp:send(Socket, PUBLISH_PKT),
    receive_byte(Socket).

receive_byte(Socket) ->
    case gen_tcp:recv(Socket, 0) of
        {ok, Bytes} ->
            io:format("[LOG] Received: ~p~n", [Bytes]);
        {error, Reason} ->
            io:format("[LOG] Tcp Closed: ~p~n", [Reason])
    end.