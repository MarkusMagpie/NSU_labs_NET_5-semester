package gui;

import dto.Location;
import dto.Place;
import dto.Weather;
import service.ApiService;

import javax.swing.*;
import java.awt.event.ActionEvent;
import java.awt.*;
import java.util.concurrent.CompletableFuture;
import java.util.List;

public class PlacesGUI extends JFrame {
    private final ApiService apiService = new ApiService();

    private JTextField searchField;
    private JButton searchButton;
    private JList<Location> locationList;
    private DefaultListModel<Location> locationListModel;
    private JLabel weatherLabel;
    private JList<Place> placeList;
    private DefaultListModel<Place> placeListModel;

    public PlacesGUI() {
        super("Places");
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        setLocationRelativeTo(null);
        setSize(1500, 800);

        initComponents();
    }

    public void initComponents() {
        // сверху - searchField и кнопка Search для поиска введенного названия
        JPanel topPanel = new JPanel(new BorderLayout(5, 5));
        searchField = new JTextField();
        searchButton = new JButton("Search");
        topPanel.add(searchField, BorderLayout.CENTER);
        topPanel.add(searchButton, BorderLayout.EAST);

        // слева - Locations
        locationListModel = new DefaultListModel<>();
        locationList = new JList<>(locationListModel);
        JScrollPane locScroll = new JScrollPane(locationList);
        locScroll.setBorder(BorderFactory.createTitledBorder("Locations"));

        // Weather
        JPanel detailPanel = new JPanel(new BorderLayout(5,5));
        weatherLabel = new JLabel("Weather: ");
        detailPanel.add(weatherLabel, BorderLayout.NORTH);

        placeListModel = new DefaultListModel<>();
        placeList = new JList<>(placeListModel);
        JScrollPane placeScroll = new JScrollPane(placeList);
        placeScroll.setBorder(BorderFactory.createTitledBorder("Sights"));
        detailPanel.add(placeScroll, BorderLayout.CENTER);

        JSplitPane splitPane = new JSplitPane(
                JSplitPane.HORIZONTAL_SPLIT, locScroll, detailPanel
        );
        splitPane.setDividerLocation(200);

        // Layout
        setLayout(new BorderLayout(5,5));
        add(topPanel, BorderLayout.NORTH);
        add(splitPane, BorderLayout.CENTER);

        // Listeners
        searchButton.addActionListener(this::onSearch);
        locationList.addListSelectionListener(e -> {
            if (!e.getValueIsAdjusting()) {
                Location sel = locationList.getSelectedValue();
                if (sel != null) loadDetails(sel);
            }
        });
    }

    public void onSearch(ActionEvent e) {
        String query = searchField.getText().trim();
        if (query.isEmpty()) return;

        // clear
        locationListModel.clear();
        placeListModel.clear();
        weatherLabel.setText("Weather: ");

        apiService.searchLocations(query)
                .thenAccept(locations -> SwingUtilities.invokeLater(() -> {
                    for (Location loc : locations) {
                        locationListModel.addElement(loc);
                    }
                }))
                .exceptionally(ex -> {
                    ex.printStackTrace();
                    return null;
                });
    }

    private void loadDetails(Location loc) {
        CompletableFuture<Weather> wf = apiService.getWeather(loc);
        CompletableFuture<List<Place>> pf = apiService.getPlaces(loc);

        wf.thenAccept(weather -> SwingUtilities.invokeLater(() ->
                weatherLabel.setText(String.format("Weather: %.3f°C, %s", weather.getTemperature(), weather.getDescription()))
        ));

        pf.thenAccept(places -> SwingUtilities.invokeLater(() -> {
            placeListModel.clear();
            for (Place p : places) {
                placeListModel.addElement(p);
            }
        }))
        .exceptionally(ex2 -> {
            ex2.printStackTrace();
            return null;
        });
    }
}
