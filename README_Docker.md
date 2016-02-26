 Building and OpenELP in Docker

## Building your own Docker image
1. Create and build in build/ directory per [readme](README.md)
2. Modify doc/ELProxy.conf, setting a unique password
3. docker build -t openelp .

## Running the Docker image
```
docker run -p 8100:8100 -p 5198:5198/udp -p 5199:5199/udp -d openelp
```


## Troubleshooting
>>>>>>> fcf482c07d26bbb98d9b9b9d1db1356711fc568a
If you forgot to set a password, you'll be greeted with this error message in the log, and the container will exit.
```
OpenELP 0.6.0
Error: Missing password
Failed to load config from '/etc/ELProxy.conf' (22): Invalid argument
```
