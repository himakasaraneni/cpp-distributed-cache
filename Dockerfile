FROM eclipse-temurin:21-jdk-jammy AS build
WORKDIR /workspace
COPY src src
RUN mkdir -p out/main out/test \
    && javac --release 21 -d out/main $(find src/main/java -name '*.java') \
    && javac --release 21 -cp out/main -d out/test $(find src/test/java -name '*.java') \
    && java -ea -cp out/main:out/test com.distributedcache.LruCacheTest \
    && jar --create --file distributed-cache.jar \
       --main-class com.distributedcache.CacheServer -C out/main .

FROM eclipse-temurin:21-jre-jammy
RUN apt-get update && apt-get install -y --no-install-recommends wget \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 cache
WORKDIR /app
COPY --from=build /workspace/distributed-cache.jar /app/distributed-cache.jar
USER cache
EXPOSE 8080
ENTRYPOINT ["java", "-jar", "/app/distributed-cache.jar"]
