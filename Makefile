# SAKURA-II — single-command launcher.
#
# Prerequisites: docker >= 24 with the compose v2 plugin.
#
#   make run    build all Docker images and start every service in the background
#   make stop   stop and remove containers (named volumes are preserved)
#   make logs   stream combined logs from all running services (Ctrl-C to exit)
#   make clean  stop containers AND remove named volumes (destructive)
#
# Gazebo GUI: on a desktop with X11, the Gazebo 3D window opens automatically.
#   On a headless server (no DISPLAY), Gazebo runs server-only (no window).
#   Requires x11-xserver-utils (xhost) on the host: apt install x11-xserver-utils

.PHONY: run stop logs clean

run:
	@if [ -n "$$DISPLAY" ]; then \
		echo "X11 display detected ($$DISPLAY) — granting Docker X11 access..."; \
		xhost +local:docker 2>/dev/null || echo "  (xhost not found — install x11-xserver-utils if the Gazebo window does not appear)"; \
	else \
		echo "No DISPLAY set — Gazebo will run headless (server-only)."; \
	fi
	docker compose up --build -d
	@echo ""
	@echo "Stack is starting. Current service status:"
	@docker compose ps
	@echo ""
	@if [ -n "$$DISPLAY" ]; then \
		echo "  Gazebo 3D view     →  opens automatically (X11 forwarded)"; \
	else \
		echo "  Gazebo             →  headless (no DISPLAY set)"; \
	fi
	@echo "  Ground station API →  http://localhost:8080/api/time"
	@echo "  UDP telemetry in   →  localhost:10000"
	@echo ""
	@echo "  make logs   — stream all service logs"
	@echo "  make stop   — stop the stack"

stop:
	docker compose down --timeout 10

logs:
	docker compose logs -f

clean:
	docker compose down --timeout 10 -v --remove-orphans
