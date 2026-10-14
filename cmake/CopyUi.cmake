# Copies the built web UI next to the executable, if it has been built.
if(EXISTS "${SRC}/bar.html")
  file(COPY "${SRC}/" DESTINATION "${DST}")
else()
  message(WARNING "Web UI not built (${SRC}/bar.html missing). Run 'npm install && npm run build' in ui/.")
endif()
