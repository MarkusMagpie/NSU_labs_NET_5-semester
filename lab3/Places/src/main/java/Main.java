import dto.*;
import service.ApiService;
import gui.PlacesGUI;

import java.util.List;
import java.util.Scanner;
import java.util.concurrent.CompletableFuture;
import java.util.stream.Collectors;

/*
1 Все вызовы должны делаться с помощью HTTP-библиотеки с асинхронных интерфейсом;
2 Все независимые друг от друга вызовы API должны работать одновременно;
3 Вызовы API, которые зависят от данных, полученных из предыдущих API, должны оформляться в виде асинхронной цепочки
    вызовов;
4 Не допускаются блокировки на ожидании промежуточных результатов в цепочке вызовов, допустима только блокировка на
    ожидании конечного результата (в случае консольного приложения). Другими словами, вся логика программы должна быть
    оформлена как одна функция, которая возвращает CompletableFuture (или аналог в вашем ЯП) без блокировок.
 */

public class Main {
    public static void main(String[] args) {
        Scanner scanner = new Scanner(System.in);
        ApiService apiService = new ApiService();

//        PlacesGUI gui = new PlacesGUI();
//        gui.setVisible(true);

        System.out.print("Please enter a name of place: ");
        String query = scanner.nextLine();

        apiService.searchLocations(query)
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
            .thenCompose(selectedLocation -> {
                System.out.printf("\nYou chose: %s (%.3f, %.3f)\n", selectedLocation.getName(), selectedLocation.getLattitude(), selectedLocation.getLongitude());
                return apiService.getWeather(selectedLocation)
                    .thenApply(weather -> {
                        System.out.println("Current weather in " + selectedLocation.getName() + ": " + weather);
                        return selectedLocation; // возврат локациии для будущих операций
                    });
            })
            .thenCompose(selectedLocation -> apiService.getPlaces(selectedLocation)
                .thenCompose(places -> {
                    // 1 создание списка асинхронных вызовов
                    List<CompletableFuture<Descriptions>> detailsFutures = places.stream()
                        .map(apiService::getDescription) // то есть для каждого места создается асинхронный вызов
                        .collect(Collectors.toList());

                    // 2 ожидание завершения всех асинхронных вызовов
                    // allDone - просто переменная флаг, что все асинхронные вызовы завершены. Не содержит результатов
                    CompletableFuture[] array = new CompletableFuture[0]; // allOf() требует массив а не список
                    CompletableFuture<Void> allDone = CompletableFuture.allOf(
                        detailsFutures.toArray(array)
                    );

                    // 3 когда все асинхронные вызовы завершены, возвращается список результатов -> join() здесь не блокирует, а просто возвращает результат!!!
                    // результат хранится в detailsFutures, не в allDone
                    return allDone.thenApply(v ->
                        detailsFutures.stream()
                            .map(CompletableFuture::join) // join() возвращает результат для каждого асинхронного вызова
                            .collect(Collectors.toList()) // собираем все результаты в один список
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
            .join();

        scanner.close();
    }
}