def test_app(dut):
    # dut.expect_exact('Press ENTER to see the list of tests')
    # dut.write('[basic]')
    # dut.expect_unity_test_output(timeout=30)
    dut.run_all_single_board_cases(group="basic")