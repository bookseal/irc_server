FROM	ubuntu:latest

RUN		apt-get update && \
			apt-get install -y inspircd irssi tcpflow dumb-init netcat-traditional

RUN		mkdir -p /var/run/inspircd

COPY	run.sh .

RUN		chmod 777 run.sh

ENTRYPOINT	[ "dumb-init", "--" ]

CMD			[ "./run.sh" ]