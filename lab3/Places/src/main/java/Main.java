import dto.Location;
import service.ApiService;

import java.util.List;
import java.util.Scanner;
import java.util.concurrent.CompletableFuture;

public class Main {
    public static void main(String[] args) {
        Scanner scanner = new Scanner(System.in);
        ApiService apiService = new ApiService();

        System.out.print("Please enter a name of place: ");
        String query = scanner.nextLine();

        CompletableFuture<List<Location>> locationsFuture = apiService.searchLocations(query);

        locationsFuture
                .thenApply(locations -> {
                    System.out.println("\nFound locations:");
                    for (int i = 0; i < locations.size(); i++) {
                        Location loc = locations.get(i);
                        System.out.printf("%d - %s (%.3f, %.3f)\n", i + 1, loc.getName(), loc.getLattitude(), loc.getLongitude());
                    }
                    return locations;
                })
                .thenApply(locations -> {
                    System.out.print("\nChoose one of given location numbers: ");
                    int choice = scanner.nextInt();
                    return locations.get(choice - 1);
                })
                .thenCompose(selectedLocation -> {
                    System.out.printf("\nYou chose: %s (%.3f, %.3f)\n", selectedLocation.getName(), selectedLocation.getLattitude(), selectedLocation.getLongitude());
                    return apiService.getWeather(selectedLocation)
                            .thenApply(weather -> {
                                System.out.println("Current weather in " + selectedLocation.getName() + ": " + weather);
                                return selectedLocation; // возврат локациии для будущих операций
                            });
                })
                .exceptionally(ex -> {
                    System.out.println("Error: " + ex.getMessage());
                    return null;
                })
                .join(); // блок только здесь для ожидания результата

        scanner.close();
    }
}