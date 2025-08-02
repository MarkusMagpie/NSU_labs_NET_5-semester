import dto.*;
import service.ApiService;
import gui.PlacesGUI;

import java.util.List;
import java.util.Scanner;
import java.util.concurrent.CompletableFuture;
import java.util.stream.Collectors;

public class Main {
    public static void main(String[] args) {
        Scanner scanner = new Scanner(System.in);
        ApiService apiService = new ApiService();

//        PlacesGUI gui = new PlacesGUI();
//        gui.setVisible(true);

        System.out.print("Please enter a name of place: ");
        String query = scanner.nextLine();

        apiService.searchLocations(query)
                // 1 вывожу + выбираю локацию
                .thenApply(locations -> {
                    System.out.println("\nFound locations:");
                    for (int i = 0; i < locations.size(); i++) {
                        Location loc = locations.get(i);
                        System.out.printf("%d - %s (%.3f, %.3f)\n", i + 1, loc.getName(), loc.getLattitude(), loc.getLongitude());
                    }
                    System.out.print("\nChoose one of given location numbers: ");
                    int choice = scanner.nextInt();
                    return locations.get(choice - 1);
                })
                // 2 получаю + вывожу погоду
                .thenCompose(selectedLocation -> {
                    System.out.printf("\nYou chose: %s (%.3f, %.3f)\n", selectedLocation.getName(), selectedLocation.getLattitude(), selectedLocation.getLongitude());
                    return apiService.getWeather(selectedLocation)
                            .thenApply(weather -> {
                                System.out.println("Current weather in " + selectedLocation.getName() + ": " + weather);
                                return selectedLocation; // возврат локациии для будущих операций
                            });
                })
                // 3 получаю + вывожу интересные места
                .thenCompose(selectedLocation -> apiService.getPlaces(selectedLocation)
                        .thenCompose(places -> {
                            // список для получения деталей каждого места
                            List<CompletableFuture<Descriptions>> detailsFutures = places.stream()
                                    .map(place -> apiService.getDescription(place))
                                    .collect(Collectors.toList());

                            // преобразование списка
                            CompletableFuture<Void> allDone = CompletableFuture.allOf(
                                    detailsFutures.toArray(new CompletableFuture[0])
                            );

                            return allDone.thenApply(v ->
                                    detailsFutures.stream()
                                            .map(CompletableFuture::join)
                                            .collect(Collectors.toList())
                            );
                        })
                        .thenApply(placeDetailsList -> {
                            System.out.println("\nSights near " + selectedLocation.getName() + ":");
                            for (int i = 0; i < placeDetailsList.size(); i++) {
                                Descriptions descriptions = placeDetailsList.get(i);
                                System.out.printf("%d - %s\n", i + 1, descriptions);
                            }
                            return selectedLocation;
                        })
                )
                .exceptionally(ex -> {
                    System.out.println("Error: " + ex.getMessage());
                    return null;
                })
                .join(); // блок только здесь для ожидания результата

        scanner.close();
    }
}