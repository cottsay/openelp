##Building and Running OpenELP in Docker
This covers the steps needed to build a Docker container for OpenELP, based on the official Centos image in Docker Hub.

We will probably push an official image to Docker Hub once we finish these outstanding investigations:
* Set a password as the container is launched, instead of putting it in ELConfig.conf and rebuilding the Docker image
* Try deploying it through Docker Cloud - this should work with no compile or VM setup required

### Building your own Docker image
1. Create and build in build/ directory per [readme](README.md)
2. Modify doc/ELProxy.conf, setting a unique password
3. docker build -t openelp .

### Running the Docker image
```
docker run -p 8100:8100 -p 5198:5198/udp -p 5199:5199/udp -d openelp
```


### Troubleshooting
If you forgot to set a password, you'll be greeted with this error message in the log, and the container will exit.
```
OpenELP 0.6.0
Error: Missing password
Failed to load config from '/etc/ELProxy.conf' (22): Invalid argument
```
