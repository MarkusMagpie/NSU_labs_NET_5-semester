package service;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import dto.*;


import java.net.URI;
import java.net.http.*;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CompletableFuture;

public class ApiService {
    private final HttpClient httpClient = HttpClient.newHttpClient();
    private final ObjectMapper mapper = new ObjectMapper();

    // получение списка локаций
    public CompletableFuture<List<Location>> searchLocations(String query) {
        String url = "https://graphhopper.com/api/1/geocode?q=" + query + "&key=" + GEOKODE_API_KEY;
        HttpRequest request = HttpRequest.newBuilder().
                uri(URI.create(url)).
                build();

        return httpClient.sendAsync(request, HttpResponse.BodyHandlers.ofString())
                .thenApply(response -> {
                    return parseLocations(response.body());
                });
    }

    /*
    "hits": [
    {
        "name": "Москва",
        "point": {
            "lat": x,
            "lng": y
        },
        "...": ...
    },
    {...}
    ]
     */
    public List<Location> parseLocations(String response) {
        try {
            List<Location> locations = new ArrayList<>();
            JsonNode root = mapper.readTree(response);
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

    // получение погоды по координатам
    public CompletableFuture<Weather> getWeather(Location location) {
        String url = "https://api.openweathermap.org/data/2.5/weather?lat="
                + location.getLattitude()
                + "&lon=" + location.getLongitude()
                + "&appid=" + WEATHER_API_KEY
                + "&units=metric";
        HttpRequest request = HttpRequest.newBuilder().
                uri(URI.create(url)).
                build();

        return httpClient.sendAsync(request, HttpResponse.BodyHandlers.ofString())
                .thenApply(response -> {
                    return parseWeather(response.body());
                });
    }

    // пример JSON ответа и почему парсинг именно такой смотри здесь: https://openweathermap.org/current
    public Weather parseWeather(String response) {
        try {
            JsonNode root = mapper.readTree(response);
            double temp = root.path("main").path("temp").asDouble();
            String desc = root.path("weather").get(0).path("description").asText();

            return new Weather(temp, desc);
        } catch (Exception e) {
            throw new RuntimeException("JSON parsing error", e);
        }
    }

    // получение списка интеренсых мест в локации
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

    private List<Place> parsePlaces(String json) {
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

    // получение подробного описания места по xid
    public CompletableFuture<Descriptions> getPlaceDetail(String xid) {
        // TODO
        return null;
    }

    public CompletableFuture<Result> searchAndFetchAll(String query) {
        // TODO
        return null;
    }
}
