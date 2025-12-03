package snakegame.io.datatypes;

import lombok.Builder;
import lombok.Getter;
import lombok.Setter;
import me.ippolitov.fit.snakes.SnakesProto;

@Builder
public class MessageWithSender {
    @Getter
    private SnakesProto.GameMessage message;
    @Getter
    private String ip;
    @Getter
    private Integer port;
    @Getter
    @Setter
    private boolean onlyView;
}
