FROM ubuntu:bionic

RUN apt-get update
RUN apt-get install cmake libssl-dev libboost-all-dev libcpprest-dev -y

WORKDIR /app
COPY . .

CMD ["bash"]
