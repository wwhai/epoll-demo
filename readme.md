# C Epoll TCP Server

## 编译运行
```sh
git clone ...
cd tcps
makes
./tcps -c config.ini
```
## TCP 测试
```erlang
    lists:foreach(fun(I)->
        io:format("~p\r\n", [I]),
        {ok, Sock} = gen_tcp:connect("106.15.225.172", 2889, [binary, {packet, 0}]),
        ok = gen_tcp:send(Sock, "OK"),
        ok = gen_tcp:close(Sock)
    end, lists:seq(1,10)).
```