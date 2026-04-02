#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${ROOT}/build"
SOCKET_PATH="${SOCKET_PATH:-/tmp/ipc.sock}"
LOG_PATH="${ROOT}/server.log"
PID_DIR="${ROOT}/.runtime"
SERVER_PID_FILE="${PID_DIR}/app2_server.pid"

APP2="${BUILD}/app2_server/app2_server"
APP1="${BUILD}/app1_cli/app1_cli"

mkdir -p "${PID_DIR}"

build() {
    cmake -S "${ROOT}" -B "${BUILD}" -DCMAKE_BUILD_TYPE=Release
    cmake --build "${BUILD}" -j"$(nproc 2>/dev/null || echo 4)"
}

is_running() {
    local f="$1"
    [[ -f "$f" ]] || return 1
    local pid
    pid="$(cat "$f")"
    [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null
}

stop_one() {
    local f="$1"
    local name="$2"
    if ! is_running "$f"; then
        rm -f "$f"
        return 0
    fi
    local pid
    pid="$(cat "$f")"
    echo "Stopping ${name} (PID ${pid})..."
    kill -TERM "${pid}" 2>/dev/null || true
    local i=0
    while kill -0 "${pid}" 2>/dev/null && [[ $i -lt 50 ]]; do
        sleep 0.1
        i=$((i + 1))
    done
    if kill -0 "${pid}" 2>/dev/null; then
        echo "Force kill ${name} (PID ${pid})"
        kill -KILL "${pid}" 2>/dev/null || true
    fi
    rm -f "$f"
}

cmd_start() {
    build
    if is_running "${SERVER_PID_FILE}"; then
        echo "Server already running (PID $(cat "${SERVER_PID_FILE}"))."
        exit 1
    fi
    rm -f "${SOCKET_PATH}"
    "${APP2}" "${SOCKET_PATH}" "${LOG_PATH}" &
    echo $! >"${SERVER_PID_FILE}"
    echo "Server started, PID $(cat "${SERVER_PID_FILE}") (socket ${SOCKET_PATH})."
}

cmd_stop() {
    stop_one "${SERVER_PID_FILE}" "app2_server"
    pkill -f "${APP1}" 2>/dev/null || true
    rm -f "${PID_DIR}/app1_cli.pid"
    rm -f "${SOCKET_PATH}"
    echo "Stopped."
}

cmd_run() {
    pkill -f "${BUILD}/app2_server/app2_server" 2>/dev/null || true
    rm -f "${SOCKET_PATH}"

    build
    "${APP2}" "${SOCKET_PATH}" "${LOG_PATH}" &
    RUN_SERVER_PID=$!
    trap 'cleanup_session "${RUN_SERVER_PID}"' EXIT INT TERM
    sleep 0.1
    "${APP1}" "${SOCKET_PATH}"
}

cleanup_session() {
    local pid="${1:-}"
    if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
        kill -TERM "${pid}" 2>/dev/null || true
        local i=0
        while kill -0 "${pid}" 2>/dev/null && [[ $i -lt 50 ]]; do
            sleep 0.1
            i=$((i + 1))
        done
        if kill -0 "${pid}" 2>/dev/null; then
            kill -KILL "${pid}" 2>/dev/null || true
        fi
        wait "${pid}" 2>/dev/null || true
    fi
    rm -f "${SOCKET_PATH}"
}

usage() {
    cat <<EOF
Usage: $(basename "$0") [command]

Commands:
  (none)  — собрать, запустить сервер и CLI; по выходу из CLI убить сервер
  run     — то же, что без аргумента
  build   — только CMake + сборка
  start   — собрать и держать сервер в фоне (PID в ${SERVER_PID_FILE})
  stop    — остановить сервер (PID-файл), завершить app1_cli этой сборки (pkill -f), убрать сокет

Переменная окружения: SOCKET_PATH (по умолчанию ${SOCKET_PATH})
EOF
}

case "${1:-run}" in
    run) cmd_run ;;
    build)  build ;;
    start)  cmd_start ;;
    stop)   cmd_stop ;;
    -h|--help) usage ;;
    *)      usage; exit 1 ;;
esac
