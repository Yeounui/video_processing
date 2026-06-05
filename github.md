# GitHub Release Notes

이 프로젝트를 `Yeounui/video_processing`에 Qt UI 버전으로 올릴 때 사용한 명령어 정리입니다.

## 원격 저장소와 브랜치

```bash
git remote add origin https://github.com/Yeounui/video_processing.git
git switch -c qt-ui-version
```

이미 `origin`이 있으면 확인만 합니다.

```bash
git remote -v
```

## GitHub 인증

push가 403으로 막히면 현재 인증 상태와 repo 권한을 확인합니다.

```bash
gh auth status
gh repo view Yeounui/video_processing --json nameWithOwner,isPrivate,viewerPermission,defaultBranchRef
```

`viewerPermission`이 `ADMIN`인데도 `git push`가 403이면 Git이 오래된 HTTPS credential을 쓰는 경우가 많습니다. 다시 로그인하고 Git credential helper를 갱신합니다.

```bash
gh auth logout -h github.com
gh auth login
gh auth setup-git
```

그 다음 다시 push합니다.

```bash
git push -u origin qt-ui-version
```

## 커밋과 Push

커밋 전 상태를 확인합니다.

```bash
git status --short
git diff --cached --stat
git diff --cached --check
```

커밋하고 브랜치를 올립니다.

```bash
git commit -m "Prepare Qt UI preview release"
git push -u origin qt-ui-version
```

추가 커밋은 이미 upstream이 잡혀 있으므로 `git push`만 실행하면 됩니다.

```bash
git add <files>
git commit -m "Add Qt UI preview assets"
git push
```

## Release 생성

브랜치가 올라간 뒤 release tag를 만듭니다.

```bash
gh release create v0.2.0-qt-ui \
    --target qt-ui-version \
    --title "Qt UI Preview" \
    --notes "Qt 6 Quick/QML based desktop UI version of video_processing."
```

Release URL 확인:

```bash
gh release view v0.2.0-qt-ui --web
```

## Release 태그를 최신 커밋으로 옮겨야 할 때

이미 release를 만든 뒤 README, assets, build fix 같은 커밋을 추가했다면 새 patch 태그를 만드는 편이 가장 안전합니다.

```bash
gh release create v0.2.1-qt-ui \
    --target qt-ui-version \
    --title "Qt UI Preview v0.2.1" \
    --notes "Adds preview assets and build fixes for the Qt UI branch."
```

기존 `v0.2.0-qt-ui` 태그를 강제로 옮기는 방법도 있지만, 이미 공개한 release를 바꾸는 작업이라 일반적으로는 새 태그를 권장합니다.
