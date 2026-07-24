-- One-click first-run helper for the unsigned macOS build of Loupe.
-- Removes only the download-quarantine flag from Loupe.app and launches it.
-- Compile to an app with:  osacompile -o "Open Loupe.app" unblock-loupe.applescript
on run
	set searchDirs to {}
	try
		set enclosing to do shell script "dirname " & quoted form of (POSIX path of (path to me))
		set end of searchDirs to enclosing & "/Loupe.app"
	end try
	set end of searchDirs to (POSIX path of (path to home folder)) & "Downloads/Loupe.app"
	set end of searchDirs to "/Applications/Loupe.app"

	set loupePath to ""
	repeat with candidate in searchDirs
		if (do shell script "test -d " & quoted form of candidate & " && echo yes || echo no") is "yes" then
			set loupePath to candidate as text
			exit repeat
		end if
	end repeat

	if loupePath is "" then
		set chosen to choose file with prompt "Select Loupe.app" of type {"com.apple.application-bundle"}
		set loupePath to POSIX path of chosen
	end if

	do shell script "xattr -dr com.apple.quarantine " & quoted form of loupePath
	do shell script "open " & quoted form of loupePath
	display notification "Loupe is unblocked and opening." with title "Loupe"
end run
