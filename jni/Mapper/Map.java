
package mapper;

import mapper.map.*;
import mapper.Value;
import java.util.Iterator;

public class Map
{
    public class Slot {
        /* constructor */
        public Slot(long slot) {
            _slot = slot;
        }

        /* properties */
        public native Value property(String property);
        public native Slot setProperty(String property, Value p);
        public native Slot removeProperty(String property);

        private native int mapperSlotBoundMax(long slot);
        public BoundaryAction boundMax() {
            return BoundaryAction.values()[mapperSlotBoundMax(_slot)];
        }
        private native void mapperSetSlotBoundMax(long slot, int value);
        public Slot setBoundMax(BoundaryAction action) {
            mapperSetSlotBoundMax(_slot, action.value());
            return this;
        }

        private native int mapperSlotBoundMin(long slot);
        public BoundaryAction boundMin() {
            return BoundaryAction.values()[mapperSlotBoundMin(_slot)];
        }
        private native void mapperSetSlotBoundMin(long slot, int value);
        public Slot setBoundMin(BoundaryAction action) {
            mapperSetSlotBoundMin(_slot, action.value());
            return this;
        }

        public native boolean calibrating();
        public native Slot setCalibrating(boolean calibrating);

        public native boolean causesUpdate();
        public native Slot setCausesUpdate(boolean causeUpdate);

        public native Value maximum();
        public native Slot setMaximum(Value p);

        public native Value minimum();
        public native Slot setMinimum(Value p);

        public native boolean useAsInstance();
        public native Slot setUseAsInstance(boolean useAsInstance);

        public native Signal signal();

        private long _slot;
    }

    private native long mapperMapSrcSlotPtr(long p, int i);
    private native long mapperMapDstSlotPtr(long p);

    /* constructor */
    public Map(long map) {
        _map = map;
        if (_map == 0)
            return;

        _num_sources = this.numSources();
        sources = new Slot[_num_sources];
        for (int i = 0; i < _num_sources; i++)
            sources[i] = new Slot(mapperMapSrcSlotPtr(_map, i));
        destination = new Slot(mapperMapDstSlotPtr(_map));
    }

    private native long mapperMapNew(mapper.Signal[] _sources,
                                     mapper.Signal _destination);
    public Map(mapper.Signal[] _sources, mapper.Signal _destination) {
        _map = mapperMapNew(_sources, _destination);
        if (_map == 0)
            return;

        _num_sources = this.numSources();
        sources = new Slot[_num_sources];
        for (int i = 0; i < _num_sources; i++)
            sources[i] = new Slot(mapperMapSrcSlotPtr(_map, i));
        destination = new Slot(mapperMapDstSlotPtr(_map));
    }
    public Map(mapper.Signal _source, mapper.Signal _destination) {
        mapper.Signal[] temp = new Signal[1];
        temp[0] = _source;
        _map = mapperMapNew(temp, _destination);
        if (_map == 0)
            return;

        _num_sources = this.numSources();
        sources = new Slot[_num_sources];
        for (int i = 0; i < _num_sources; i++)
            sources[i] = new Slot(mapperMapSrcSlotPtr(_map, i));
        destination = new Slot(mapperMapDstSlotPtr(_map));
    }

    /* push */
    public native Map push();

    /* properties */
    public native Value property(String property);
    public native Map setProperty(String property, Value p);
    public native Map removeProperty(String property);

    /* property: expression */
    public native String expression();
    public native Map setExpression(String expression);

    /* property: id */
    public native long id();

    /* property: mode */
    private native int mapperMapMode(long map);
    public Mode mode() {
        return Mode.values()[mapperMapMode(_map)];
    }
    private native void mapperSetMode(long map, int mode);
    public Map setMode(Mode mode) {
        mapperSetMode(_map, mode.value());
        return this;
    }

    /* property: muted */
    public native boolean muted();
    public native Map setMuted(boolean muted);

    /* property: scopes */
    public native mapper.device.Query scopes();
    public native Map addScope(mapper.Device dev);
    public native Map removeScope(mapper.Device dev);

    /* property: numSources */
    public native int numSources();

    /* property: ready */
    public native boolean ready();

    public Slot source(int index) {
        if (index > _num_sources)
            return null;
        return sources[index];
    }
    public Slot source() {
        return source(0);
    }

    public Slot destination() {
        return destination;
    }

    public Slot[] sources;
    public Slot destination;
    private int _num_sources;
    private long _map;
}
