##Building and Running OpenELP in Docker
This covers the steps needed to build a Docker container for OpenELP, based on the official Centos image in Docker Hub.

### Running the Docker image
There will be a premade Docker image for OpenELP available on Docker Hub shortly. If you have [Docker](http://www.docker.com) installed, all you need is one command to pull & run OpenELP.

Be sure to replace <uniquepassword> with the password you would actually like to use
```
docker run -p 8100:8100 -p 5198:5198/udp -p 5199:5199/udp -d -e "password=<uniquepassword>" openelp
```



### Building your own Docker image
1. Create and build in build/ directory per [readme](README.md)
2. Modify doc/ELProxy.conf, setting a unique password
3. docker build -t openelp .


### Troubleshooting
If you don't set a password as shown above, you'll be greeted with this error message in the log, and the container will exit.
```
OpenELP 0.6.0
Error: Missing password
Failed to load config from '/etc/ELProxy.conf' (22): Invalid argument
```
