"""
Investigate the docs. Investigate the claude related files and folders. Are there any missing doc in this project? Investigate our target project with docs and find the gaps. After that create a plan for implement this docs. Find the next steps and add them to plan and add next steps to docs/README.md. Update items progresses in docs/README.md. Ask your questions if necessarry   
"""

"""

🛰️ SAKURA-II Universal Phase Execution Prompt (v1.0)

"Role: Act as the Lead Systems Engineer (Opus) or Implementation Engineer (Sonnet) as specified in the Master Matrix.

Task: Execute Phase [FAZ_NUMARASI] as defined in IMPLEMENTATION_GUIDE.md.

Operational Protocol:

    Audit & Context Sync: - Read the specific Phase Card in IMPLEMENTATION_GUIDE.md.

        Regression Check: Briefly review the deliverables of the previous 2-3 phases to ensure architectural continuity and dependency alignment.

        Cross-reference the cited Source of Truth documents, the REPO_MAP.md, and current Decision Logs.

    Git: Execute the phase in the prescribed branch: [BRANCH_ADI].

    TDD Workflow (Red-Green-Refactor):

        Red: Write a Given/When/Then test that fails, proving the requirement is not yet met.

        Green: Implement the minimal, mission-critical code to satisfy the test.

        Refactor: Optimize for no_std, zero-panic, and 100% coverage while following .claude/rules/.

    Compliance Audit: Verify that Q-C8 (Endianness) and Q-F3 (Radiation Anchors) are strictly respected.

    Memory & Progress Loop: - If any architectural decisions occur, update docs/standards/decisions-log.md.

        CRITICAL: Upon successful completion of all DoD criteria, you MUST update the Master Implementation Matrix in IMPLEMENTATION_GUIDE.md by marking the corresponding phase's checkbox as checked: [x].

Output Requirement:
Before writing any code, provide a Pre-Flight Report:

    Regression Status: Does this phase require changes to previous work? (Yes/No - Explain).

    Files & Deliverables: List of files to be created or modified (aligned with REPO_MAP.md).

    The 'Red' Test: Define the specific Given/When/Then scenario for the first test.

    AI Strategy: Which specific skills, docs, or agents from the 'AI Prompting Strategy' section will be used?

Do you have any questions before we initiate the Git branch and begin the Red step?"

"""