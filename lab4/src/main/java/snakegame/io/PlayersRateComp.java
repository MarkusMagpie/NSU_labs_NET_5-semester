package snakegame.io;

import java.util.Comparator;

public class PlayersRateComp implements Comparator<me.ippolitov.fit.snakes.SnakesProto.GamePlayer>{
    @Override
    public int compare(me.ippolitov.fit.snakes.SnakesProto.GamePlayer o1, me.ippolitov.fit.snakes.SnakesProto.GamePlayer o2) {
        return o2.getScore() - o1.getScore();
    }
}
