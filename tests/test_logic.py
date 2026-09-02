import sys
from pathlib import Path
from types import SimpleNamespace
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "raspberry_pi"))
from trafficlight.traffic.state_machine import StateMachine, TrafficState
from trafficlight.traffic.pair_detector import DirectionalPairDetector


def snap(*, occ=False, rising=None, online=True):
    return SimpleNamespace(occupied=occ, rising_edge_at=rising, online=online)


def pair_inputs(s1=False, s2=False, r1=None, r2=None, online=True):
    return {"S1": snap(occ=s1, rising=r1, online=online), "S2": snap(occ=s2, rising=r2, online=online)}


def test_s1_alone_does_not_trigger_or_activate_yellow():
    p = DirectionalPairDetector("S1", "S2", 2.0, "YELLOW")
    r = p.update(pair_inputs(s1=True, r1=10.0), 10.0)
    assert not r.triggered
    assert not r.active


def test_correct_direction_triggers_and_latches_until_both_clear():
    p = DirectionalPairDetector("S1", "S2", 2.0, "YELLOW")
    p.update(pair_inputs(s1=True, r1=10.0), 10.0)
    r = p.update(pair_inputs(s1=True, s2=True, r1=10.0, r2=10.6), 10.6)
    assert r.triggered and r.active
    # body/dolly gaps that do not clear both remain same convoy
    assert p.update(pair_inputs(s1=False, s2=True, r1=10.0, r2=10.6), 11.0).active
    assert not p.update(pair_inputs(s1=False, s2=False, r1=10.0, r2=10.6), 12.0).active


def test_reverse_direction_is_blocked_until_clear():
    p = DirectionalPairDetector("S1", "S2", 2.0, "YELLOW")
    p.update(pair_inputs(s2=True, r2=20.0), 20.0)
    r = p.update(pair_inputs(s1=True, s2=True, r1=20.5, r2=20.0), 20.5)
    assert r.wrong_direction and not r.triggered
    # extra oscillation while still occupied cannot become a forward trigger
    r = p.update(pair_inputs(s1=True, s2=True, r1=20.5, r2=20.8), 20.8)
    assert not r.triggered
    p.update(pair_inputs(s1=False, s2=False, r1=20.5, r2=20.8), 22.0)
    p.update(pair_inputs(s1=True, r1=23.0), 23.0)
    assert p.update(pair_inputs(s1=True, s2=True, r1=23.0, r2=23.6), 23.6).triggered


def test_pair_timeout_rejects_too_slow_sequence():
    p = DirectionalPairDetector("S1", "S2", 1.0, "YELLOW")
    p.update(pair_inputs(s1=True, r1=1.0), 1.0)
    assert not p.update(pair_inputs(s1=False, s2=True, r1=1.0, r2=2.5), 2.5).triggered


def test_offline_sensor_clears_pending_sequence():
    p = DirectionalPairDetector("S1", "S2", 2.0, "YELLOW")
    p.update(pair_inputs(s1=True, r1=1.0), 1.0)
    d = pair_inputs(s1=True, r1=1.0)
    d["S2"].online = False
    p.update(d, 1.2)
    d["S2"] = snap(occ=True, rising=1.4, online=True)
    assert not p.update(d, 1.4).triggered


def test_red_fixed_then_return_then_idle():
    sm = StateMachine({"red_duration_s":3,"return_yellow_s":5,"yellow_clear_delay_s":5})
    t=100.0; sm.state_since=t
    assert sm.update(False, True, False, t) == TrafficState.RED
    assert sm.update(False, False, False, t+2.9) == TrafficState.RED
    assert sm.update(False, False, False, t+3.0) == TrafficState.RETURN
    assert sm.update(False, False, False, t+8.0) == TrafficState.IDLE


def test_return_continues_yellow_if_active_no_green_flash():
    sm = StateMachine({"red_duration_s":3,"return_yellow_s":5,"yellow_clear_delay_s":5})
    t=100.0; sm.state_since=t
    sm.update(False, True, False, t)
    sm.update(False, False, False, t+3)
    assert sm.update(False, False, True, t+8) == TrafficState.YELLOW


def test_yellow_clears_only_after_delay():
    sm = StateMachine({"red_duration_s":3,"return_yellow_s":5,"yellow_clear_delay_s":5})
    t=10.0
    assert sm.update(True, False, True, t) == TrafficState.YELLOW
    assert sm.update(False, False, False, t+1) == TrafficState.YELLOW
    assert sm.update(False, False, False, t+5.9) == TrafficState.YELLOW
    assert sm.update(False, False, False, t+6.0) == TrafficState.IDLE
