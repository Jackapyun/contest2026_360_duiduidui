# logs/ — AI Coding 日志（本队说明）

存放本队开发过程中与 AI 编程助手的完整对话日志，与作品代码一并提交。

## 本队使用的工具

本作品全程使用 **DeepSeek Harness（DSH）** 作为 AI 编程助手（CLI/Web 形态，具备工具调用、
文件编辑、真机串口/GDB 联调能力）。DSH 的会话 transcript 为
`~/.dsh/sessions/<encoded-cwd>/<session-id>/session.jsonl.zstd`，本目录下的 JSONL 由
`work/dev-loop/export_dsh_logs.py` 转换导出。

> 说明：组委会《AI Coding 日志归集手册》列举的自动采集工具为
> claude-code / opencode / codex / kiro / mimocode；DSH 不在其中，因此本队采用
> **主动导出**方式提交，字段严格遵循 `schema/event.schema.json`（v1.0），
> `tool` 字段记为 `dsh` 以如实标注来源。

## 目录结构

```text
logs/
└── Jackapyun/                       # GitHub 用户名
    ├── manifest.json                # 会话清单（session_id/日期/事件数/大小/截断数）
    └── <date>/                      # 日期 YYYY-MM-DD（UTC）
        └── dsh__<session_id>.jsonl  # 一个会话一个文件
```

## 事件格式

每行一个 JSON 事件，字段：`schema_version` / `session_id` / `team_id` / `github_login` /
`tool` / `ts`(ISO 8601 UTC) / `role`(`user`/`assistant`/`tool`/`system`) / `seq`(会话内单调递增)，
以及：

- `text`：用户/助手消息正文（完整保留）
- `thinking`：助手推理内容
- `tool_name` / `tool_call_id` / `input` / `output`：工具调用与返回
  （单个 `output` 超过 16KB 时截断并标记 `...[truncated N chars]`，其余字段完整）
- 敏感信息（`sk-*` / `ghp_*` / `Bearer *`）已正则脱敏

## 重新导出

```bash
python3 <openvela 工作区>/../work/dev-loop/export_dsh_logs.py --list      # 查看可导出会话
python3 <openvela 工作区>/../work/dev-loop/export_dsh_logs.py --confirm \
        --out <本仓>/logs                                                # 重新导出全部会话
```

> 注：当前会话在结束/落盘后才会出现在导出结果中，因此**提交前建议再执行一次导出**，
> 以纳入最后一次开发会话。
