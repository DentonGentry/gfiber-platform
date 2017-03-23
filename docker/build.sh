#!/bin/sh

cp "$(which docker)" docker
tar -czh .. | docker build -f docker/Dockerfile.build -t gfiber-cmds-build -
docker run --rm -i -t -v /var/run/docker.sock:/var/run/docker.sock \
  gfiber-cmds-build dockerize -t isoping-base /bin/isoping
docker build -t isoping .

docker tag isoping gcr.io/gfiber-isoping/isoping
gcloud docker -- push gcr.io/gfiber-isoping/isoping
