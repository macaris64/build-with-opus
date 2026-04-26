# SAKURA-II — single-command launcher.
#
# Prerequisites: docker >= 24 with the compose v2 plugin.
#
#   make run    build all Docker images and start every service in the background
#   make stop   stop and remove containers (named volumes are preserved)
#   make logs   stream combined logs from all running services (Ctrl-C to exit)
#   make clean  stop containers AND remove named volumes (destructive)
#
# Gazebo runs headless (server-only, no GUI window).

.PHONY: run stop logs clean fe inject

fe:
	cd frontend && npm run dev

inject:
	cargo run --bin demo_injector

run:
	docker compose up --build -d
	@echo ""
	@echo "Stack is starting. Current service status:"
	@docker compose ps
	@echo ""
	@echo "  Gazebo             →  headless (server-only)"
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
