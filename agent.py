from kimix import *
prompt('''
read files:
- D:/hermes-agent-cn/reports/core_agent_native_design.md
- D:/hermes-agent-cn/reports/optimization_plan.md
read all skills, understand all.

For the native-static library requirements, write a feature-list and plan to implement them in current C language project. save to .kimix_cache/plan.md
Note:
- for feature source code, save to src/<feature-name>/
- each feature need to write test to verify, tests save to src/tests/
''')
prompt('''
review .kimix_cache/plan.md again, make sure all features is listed in plan, according to:

- D:/hermes-agent-cn/reports/core_agent_native_design.md
- D:/hermes-agent-cn/reports/optimization_plan.md
''')
prompt('''
implement the plan in .kimix_cache/plan.md. make sure:

- all features implemented, match with python scripts logic defined in D:/hermes-agent-cn/
- all tests run and verified.
''')
prompt('''
Write a report:

according to last task, write all implemented features to `report.md`
''')