"""
test_sim_launch.py — Launch-description smoke test for sim.launch.py.

Validates that sim.launch.py generates a well-formed LaunchDescription
without starting any external processes (Gazebo, bridge, or teleop).
This test runs in CI without Gazebo or ROS 2 middleware installed — it
only exercises the Python launch-description parser.

Run:
    colcon test --packages-select rover_bringup
    colcon test-result --verbose
"""

import os
import sys
import unittest


class TestSimLaunchDescription(unittest.TestCase):
    """Validate sim.launch.py without executing any external processes."""

    def _load_launch(self):
        """Import and call generate_launch_description() from sim.launch.py."""
        import importlib.util

        launch_path = os.path.join(
            os.path.dirname(__file__),
            "..",
            "launch",
            "sim.launch.py",
        )
        launch_path = os.path.normpath(launch_path)

        spec = importlib.util.spec_from_file_location("sim_launch", launch_path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module.generate_launch_description()

    def test_launch_description_is_not_none(self):
        """Given sim.launch.py, When generate_launch_description() is called,
        Then it returns a non-None LaunchDescription object."""
        ld = self._load_launch()
        self.assertIsNotNone(ld)

    def test_launch_description_has_actions(self):
        """Given sim.launch.py, When the description is generated,
        Then it contains at least four entities (args + processes)."""
        ld = self._load_launch()
        # The description must have at least: 3 args + gz_server + bridge + teleop
        self.assertGreaterEqual(len(ld.entities), 4)

    def test_sdf_arg_declares_default_path(self):
        """Given sim.launch.py, When the description is generated,
        Then a 'sdf_path' launch argument is declared."""
        from launch.actions import DeclareLaunchArgument

        ld = self._load_launch()
        arg_names = [
            e.name
            for e in ld.entities
            if isinstance(e, DeclareLaunchArgument)
        ]
        self.assertIn("sdf_path", arg_names,
                      "Expected 'sdf_path' DeclareLaunchArgument in sim.launch.py")

    def test_params_file_arg_declared(self):
        """Given sim.launch.py, When the description is generated,
        Then a 'params_file' launch argument is declared."""
        from launch.actions import DeclareLaunchArgument

        ld = self._load_launch()
        arg_names = [
            e.name
            for e in ld.entities
            if isinstance(e, DeclareLaunchArgument)
        ]
        self.assertIn("params_file", arg_names)

    def test_headless_arg_declared(self):
        """Given sim.launch.py, When the description is generated,
        Then a 'headless' launch argument is declared."""
        from launch.actions import DeclareLaunchArgument

        ld = self._load_launch()
        arg_names = [
            e.name
            for e in ld.entities
            if isinstance(e, DeclareLaunchArgument)
        ]
        self.assertIn("headless", arg_names)

    def test_gz_server_process_declared(self):
        """Given sim.launch.py, When the description is generated,
        Then an ExecuteProcess entity for gz sim is present."""
        from launch.actions import ExecuteProcess

        ld = self._load_launch()
        exec_procs = [e for e in ld.entities if isinstance(e, ExecuteProcess)]
        self.assertEqual(len(exec_procs), 1,
                         "Expected exactly one ExecuteProcess (gz sim server)")

    def test_bridge_and_teleop_are_delayed(self):
        """Given sim.launch.py, When the description is generated,
        Then bridge and teleop are wrapped in TimerAction to allow Gazebo startup."""
        from launch.actions import TimerAction

        ld = self._load_launch()
        timers = [e for e in ld.entities if isinstance(e, TimerAction)]
        self.assertEqual(len(timers), 2,
                         "Expected two TimerActions (bridge + teleop)")

    def test_bridge_timer_period_is_positive(self):
        """Given sim.launch.py, When the description is generated,
        Then the bridge timer period is > 0 s (Gazebo world-load delay)."""
        from launch.actions import TimerAction

        ld = self._load_launch()
        timers = [e for e in ld.entities if isinstance(e, TimerAction)]
        for timer in timers:
            self.assertGreater(float(timer._period), 0.0)


def main():
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromTestCase(TestSimLaunchDescription)
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)


if __name__ == "__main__":
    main()
