# Отчёт по 3 лабораторной работе "Места (Асинхронное сетевое взаимодействие)"

Используя методы асинхронного программирования (например, `CompletableFuture` для 
`Java`) или библиотеки реактивного программирования (`RxJava`, например) 
провзаимодействовать с несколькими публично доступными API и сделать приложение с 
любым интерфейсом, основанное на этих API. При этом API должны использоваться так:

 - Все вызовы должны делаться с помощью HTTP-библиотеки с асинхронных интерфейсом;  
 - Все независимые друг от друга вызовы API должны работать одновременно;  
 - Вызовы API, которые зависят от данных, полученных из предыдущих API, 
должны оформляться в виде асинхронной цепочки вызовов;  
 - Не допускаются блокировки на ожидании промежуточных результатов в цепочке 
вызовов, допустима только блокировка на ожидании конечного результата 
(в случае консольного приложения). Другими словами, вся логика программы 
должна быть оформлена как одна функция, которая возвращает `CompletableFuture` 
(или аналог в вашем ЯП) без блокировок.  

Логика работы:
1. В поле ввода пользователь вводит название чего-то (например "Цветной проезд") 
и нажимает кнопку поиска;
2. Ищутся варианты локаций с помощью метода [1] и показываются пользователю в 
виде списка;
3. Пользователь выбирает одну локацию;
4. С помощью метода [2] ищется погода в локации и показывается пользователю;
5. С помощью метода [3] ищутся интересные места в локации, далее для каждого 
найденного места с помощью метода [4] ищутся описания, всё это показывается пользователю в виде списка.

## 1 класс `ApiService.java`

### 1.1 поля класса
```java
private final HttpClient httpClient = HttpClient.newHttpClient();
private final ObjectMapper mapper = new ObjectMapper();

private final String GRAPHHOPPER_API_KEY = "";
private final String OPENWEATHERMAP_API_KEY = "";
private final String OPENTRIPMAP_API_KEY = "";
```

### 1.2.1 метод `searchLocations(String query)`
```java
public CompletableFuture<List<Location>> searchLocations(String query) {
    String url = "https://graphhopper.com/api/1/geocode?q=" + query + "&key=" + GRAPHHOPPER_API_KEY;
    HttpRequest request = HttpRequest.newBuilder().
            uri(URI.create(url)).
            build();

    return httpClient.sendAsync(request, HttpResponse.BodyHandlers.ofString())
            .thenApply(response -> {
                return parseLocations(response.body());
            });
}
```
Метод `searchLocations` отвечает за асинхронный поиск вариантов локаций по 
введённому текстовому запросу `query` с помощью `GraphHopper Geocoding API`.
```java
String url = "https://graphhopper.com/api/1/geocode?q=" + query + "&key=" + GRAPHHOPPER_API_KEY;
```
`String url` собирает строку запроса подставляя в неё `query` - запрос юзера и 
`GRAPHHOPPER_API_KEY` - ключ API.

```java
HttpRequest request = HttpRequest.newBuilder().
            uri(URI.create(url)).
            build();
```
Далее создаём объект класса `HttpRequest`, указывая куда послать HTTP-запрос.
Подробнее про класс `HttpRequest` можно узнать [здесь](https://docs.oracle.com/en/java/javase/11/docs/api/java.net.http/java/net/http/HttpRequest.html).
> из документации:  
> An HttpRequest instance is built through an HttpRequest builder. An HttpRequest builder is obtained from one of the newBuilder methods. A request's URI, headers, and body can be set.

```java
return httpClient.sendAsync(request, HttpResponse.BodyHandlers.ofString())
    .thenApply(response -> {
        return parseLocations(response.body());
    });
```
Отправляю созданный выше HTTP-запрос `request` (второй аргумент говорит что 
тело ответа будет в виде строки `String`) асинхронно и получаю `return value`:
`CompletableFuture<HttpResponse<String>>`.  
При этом важно что метод не получает ответ от сервера моментально, сразу даёт 
только значение в которое позже придёт результат.
  
Когда же ответ на запрос дейсвтительно придёт, то `HttpResponse` благодаря 
`HttpResponse.BodyHandlers.ofString()` будет объектом `HttpResponse<String>`. 
Я беру его тело `response.body()` - строку `JSON`, полученную от `GraphHopper` и
вызываю метод описанный ниже - `parseLocations(json)`. 

Таким образом `thenApply` превращает `CompletableFuture<HttpResponse<String>>` 
в `CompletableFuture<List<Location>>`. Подробнее смотри [здесь](https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CompletableFuture.html#thenApply-java.util.function.Function-).

> логика работы вкратце:  
> как только исходный `CompletableFuture<HttpResponse<String>>` успешно завершится 
> и в нём появится значение, автоматически будет вызван `thenApply(полученное значение,
> ставшее теперь параметром)`.


### 1.2.2 метод `parseLocations(String json)`
```java
public List<Location> parseLocations(String json) {
    try {
        List<Location> locations = new ArrayList<>();
        JsonNode root = mapper.readTree(json);
        JsonNode hits = root.path("hits");

        for (JsonNode hit : hits) {
            String name = hit.path("name").asText();
            JsonNode point = hit.path("point");
            double lat = point.path("lat").asDouble();
            double lon = point.path("lng").asDouble();

            locations.add(new Location(name, lat, lon));
        }

        return locations;
    } catch (Exception e) {
        throw new RuntimeException("JSON parsing error", e);
    }
}
```

### 1.2.3 метод `getWeather(Location location)`
```java
public CompletableFuture<Weather> getWeather(Location location) {
    String url = "https://api.openweathermap.org/data/2.5/weather?lat="
            + location.getLattitude()
            + "&lon=" + location.getLongitude()
            + "&appid=" + OPENWEATHERMAP_API_KEY
            + "&units=metric";
    HttpRequest request = HttpRequest.newBuilder().
            uri(URI.create(url)).
            build();

    return httpClient.sendAsync(request, HttpResponse.BodyHandlers.ofString())
            .thenApply(response -> {
                return parseWeather(response.body());
            });
}
```

### 1.2.4 метод `parseWeather(String json)`
```java
public Weather parseWeather(String json) {
    try {
        JsonNode root = mapper.readTree(json);
        double temp = root.path("main").path("temp").asDouble();
        String desc = root.path("weather").get(0).path("description").asText();

        return new Weather(temp, desc);
    } catch (Exception e) {
        throw new RuntimeException("JSON parsing error", e);
    }
}
```

### 1.2.5 метод `getPlaces(Location location)`
```java
public CompletableFuture<List<Place>> getPlaces(Location location) {
    String url = "https://api.opentripmap.com/0.1/ru/places/radius"
            + "?radius=1000"
            + "&lat=" + location.getLattitude()
            + "&lon=" + location.getLongitude()
            + "&limit=10"
            + "&apikey=" + OPENTRIPMAP_API_KEY;
    HttpRequest request = HttpRequest.newBuilder().
            uri(URI.create(url)).
            build();

    return httpClient.sendAsync(request, HttpResponse.BodyHandlers.ofString())
            .thenApply(response -> {
                return parsePlaces(response.body());
            });
}
```

### 1.2.6 метод `parsePlaces(String json)`
```java
public List<Place> parsePlaces(String json) {
    try {
        List<Place> list = new ArrayList<>();
        JsonNode root = mapper.readTree(json);
        JsonNode features = root.path("features");
        for (JsonNode f : features) {
            JsonNode props = f.path("properties");
            String name = props.path("name").asText();
            String xid  = props.path("xid").asText();
            list.add(new Place(name, xid));
        }

        return list;
    } catch (Exception e) {
        throw new RuntimeException("Failed to parse places", e);
    }
}
```

### 1.2.7 метод `getDescription(Place place)`
```java
public CompletableFuture<Descriptions> getDescription(Place place) {
    String url = String.format("https://api.opentripmap.com/0.1/ru/places/xid/%s?apikey=%s",
            place.getXid(), OPENTRIPMAP_API_KEY);
    HttpRequest request = HttpRequest.newBuilder(URI.create(url)).GET().build();

    return httpClient.sendAsync(request, HttpResponse.BodyHandlers.ofString())
            .thenApply(response -> {
                String description = parseDescription(response.body());
                return new Descriptions(place.getName(), description);
            });
}
```

### 1.2.8 метод `parseDescription(String json)`
```java
public String parseDescription(String json) {
    try {
        JsonNode root = mapper.readTree(json);
        JsonNode wiki = root.path("wikipedia_extracts");
        if (wiki.has("text") && !wiki.path("text").asText().isBlank()) {
            return wiki.path("text").asText();
        }
        return "";
    } catch (Exception e) {
        throw new RuntimeException("Failed to parse place description", e);
    }
}
```

## 2 класс `Main.java`