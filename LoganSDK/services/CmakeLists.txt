# Layer 3 - services: http, AI backend/frontend and storage (files on the microSD card).
# No hardware knowledge. storage is built on the sdcard module (modules) and FatFs (libs).
# Add esp_http_client, esp-tls and similar to REQUIRES only when the code really uses them.
idf_component_register(SRCS "AI/AI_backend.c"
                            "AI/AI_frontend.c"
                            "http/http.c"
                            "storage/storage.c"
                            "storage/storage_diskio.c"
                    INCLUDE_DIRS "AI" "http" "storage"
                    REQUIRES modules utils
                    PRIV_REQUIRES libs)