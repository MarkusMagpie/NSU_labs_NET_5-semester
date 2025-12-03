package snakegame.snake;

import snakegame.io.PlayerController;

public class SnakeViewController {
    public SnakeViewController(PlayerController playerController, SnakeView snakeView) {
        playerController.getNewMessageSubject().subscribe(messageWithSender -> {
            if (messageWithSender.getMessage().hasState()) {
                snakeView.setState(messageWithSender.getMessage().getState().getState());
            }
        });
    }
}
