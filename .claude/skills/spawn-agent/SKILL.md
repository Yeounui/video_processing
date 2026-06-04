---
description: Claude agent invocation syntax — persistent vs ephemeral agents, SendMessage follow-up.
allowed-tools: Agent SendMessage
---

# Agent Lifecycle

```
Agent(team_name="<team>", name="<name>", subagent_type="<type>", prompt="...")  # persistent — reachable via SendMessage
Agent(subagent_type="<type>", prompt="...")                                      # ephemeral — no follow-up
SendMessage(to: "<name>", ...)                                                   # resume persistent agent
```

Persistent agents require both `team_name` and `name`. Use `run_in_background=true` for fan-out tasks that can proceed in parallel.

For handoff field reference (User Request, Target Scope, Blueprint, etc.), see `.claude/rules/Handoff_Packet.md`.