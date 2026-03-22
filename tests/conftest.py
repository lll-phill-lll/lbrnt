"""Общие фикстуры pytest для tests/."""

import pytest


def pytest_addoption(parser):
    parser.addoption(
        "--canonize",
        action="store_true",
        default=False,
        help=(
            "При несовпадении expect_stdout / подстрок в stdout обновить scenario.json "
            "фактическим выводом и считать тест пройденным (канонизация эталона)."
        ),
    )


@pytest.fixture(scope="session")
def canonize(request):
    return bool(request.config.getoption("--canonize"))
