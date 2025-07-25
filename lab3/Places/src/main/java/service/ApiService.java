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

    // https://graphhopper.com/dashboard/api-keys
    private final String GEOKODE_API_KEY = "c29183d1-b794-4ca7-866f-dd74f0d4020a";

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
            "lat": 55.7523,
            "lng": 37.6156
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
    public CompletableFuture<Weather> getWeather(Location location, double lattitude, double longitude) {
        // TODO
        return null;
    }

    // получение списка интеренсых мест в локации
    public CompletableFuture<List<Place>> getPlaces(double lattitude, double longitude) {
        // TODO
        return null;
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
