# as part of this example limit the CPUs where this docker can run to just 2 (the first 2 available)
DOCKER_EXAMPLE_CPUS:=0,1
DOCKER_EXAMPLE_MEMORY_LIMIT:=100m
DOCKER_EXAMPLE_FOLDER:=$(shell readlink -f ..)
HOSTNAME:=docker-userapp

all:
	@cp -fv ../example-load.sh .
	@docker stop userapp || true
	@docker build -t userapp . && \
		echo "cmonitor docker example built successfully; now running it" && \
		docker run \
			--detach \
			--interactive \
			--tty \
			--rm \
			--cpuset-cpus=$(DOCKER_EXAMPLE_CPUS) \
			--memory=$(DOCKER_EXAMPLE_MEMORY_LIMIT) \
			--volume=$(DOCKER_EXAMPLE_FOLDER):/perf \
			--hostname=$(HOSTNAME) \
			--name userapp \
			userapp && \
		echo "Example userapp docker successfully started: baremetal+container performance stats are collected in $(DOCKER_EXAMPLE_FOLDER)." && \
		echo "The docker will stop automatically after a while."
