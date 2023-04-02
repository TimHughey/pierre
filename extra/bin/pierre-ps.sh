#!/bin/env /bin/zsh

ps -Leo pid,tid,class,rtprio,ni,pri,psr,pcpu,stat,wchan:14,comm | grep pie 
