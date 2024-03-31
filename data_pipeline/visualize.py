import streamlit as st
import pandas as pd
import plotly.express as px
from datetime import datetime
import numpy as np
import os
import time
import pydeck as pdk

from commons import *


def load_data():
    # Check file modification time to decide if reload is needed
    mod_time = datetime.fromtimestamp(os.path.getmtime(SENSOR_FILE))
    if 'df_last_modified' not in st.session_state or mod_time != st.session_state.df_last_modified:
        st.session_state.df = pd.read_parquet(SENSOR_FILE)
        st.session_state.df_last_modified = mod_time
    return st.session_state.df


def plot_line_chart(df, x, y, title):
    fig = px.line(df, x=x, y=y, title=title)
    fig.update_layout(autosize=False, width=400, height=300)  # Set fixed size for plots
    return fig

def display_device_data(device):
    device_df = st.session_state.df[st.session_state.df['device'] == device].copy().tail(60)

    # Convert columns to numeric as necessary
    for column in ['avg_speed', 'max_speed', 'min_speed', 'num_cars']:
        device_df[column] = pd.to_numeric(device_df[column], errors='coerce')

    # Update plots
    st.session_state.avg_speed_plot.plotly_chart(plot_line_chart(device_df, 'timestamp', 'avg_speed', 'Avg Speed Over Time'), use_container_width=True)
    st.session_state.max_speed_plot.plotly_chart(plot_line_chart(device_df, 'timestamp', 'max_speed', 'Max Speed Over Time'), use_container_width=True)
    st.session_state.min_speed_plot.plotly_chart(plot_line_chart(device_df, 'timestamp', 'min_speed', 'Min Speed Over Time'), use_container_width=True)
    st.session_state.num_cars_plot.plotly_chart(plot_line_chart(device_df, 'timestamp', 'num_cars', 'Cars Over Time'), use_container_width=True)


SENSOR_LOCATIONS = {
    "Sensor 1": {"lat": 53.349805, "lon": -6.260310},
    "Sensor 2": {"lat": 53.344103, "lon": -6.267493},
    "Sensor 3": {"lat": 53.338167, "lon": -6.259929}
}

def main():
    st.set_page_config(layout="wide")
    st.title("Speed Bump Controller Dashboard")

    load_data()

    devices = np.sort(st.session_state.df['device'].unique())
    selected_device = st.sidebar.selectbox("Select a device", devices)
    
    if 'avg_speed_plot' not in st.session_state:
        prepare_sidebar_placeholders()

    display_device_data(selected_device)

    # Main body map
    st.write("Sensor Locations in Dublin")
    view_state = pdk.ViewState(latitude=53.349805, longitude=-6.260310, zoom=12, bearing=0, pitch=0)
    layer = pdk.Layer(
        "ScatterplotLayer",
        data=[{"position": [loc["lon"], loc["lat"]], "name": name} for name, loc in SENSOR_LOCATIONS.items()],
        get_position="position",
        get_color="[180, 0, 200, 140]",
        get_radius=100,
    )
    st.pydeck_chart(pdk.Deck(layers=[layer], initial_view_state=view_state))

    while True:
        load_data()
        display_device_data(selected_device)
        time.sleep(UPDATE_INTERVAL)

def prepare_sidebar_placeholders():
    st.session_state.avg_speed_plot = st.sidebar.empty()
    st.session_state.max_speed_plot = st.sidebar.empty()
    st.session_state.min_speed_plot = st.sidebar.empty()
    st.session_state.num_cars_plot = st.sidebar.empty()


if __name__ == '__main__':
    main()
