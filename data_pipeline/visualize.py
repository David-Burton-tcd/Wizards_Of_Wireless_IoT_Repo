import dash
from dash import dcc, html
import pandas as pd
from dash.dependencies import Input, Output
import plotly.express as px

from commons import *

app = dash.Dash(__name__)

@app.callback(Output('speed-time-chart', 'figure'),
              [Input('interval-component', 'n_intervals')])
def update_speed_chart(n):
    df = pd.read_parquet(SENSOR_FILE)
    # Convert speed to numeric values
    df['speed'] = pd.to_numeric(df['speed'])
    fig = px.line(df, x='timestamp', y='speed', title='Speed Over Time')
    return fig

# Callback to update the length chart
@app.callback(Output('length-time-chart', 'figure'),
              [Input('interval-component', 'n_intervals')])
def update_length_chart(n):
    df = pd.read_parquet(SENSOR_FILE)
    # Convert length to numeric values
    df['length'] = pd.to_numeric(df['length'])
    fig = px.line(df, x='timestamp', y='length', title='Length Over Time')
    return fig

def main():
    app.layout = html.Div([
        html.H1("Real-Time Data Visualization"),
        dcc.Interval(
            id='interval-component',
            interval=5*1000,  # in milliseconds (e.g., 5 seconds)
            n_intervals=0
        ),
        dcc.Graph(id='speed-time-chart'),
        dcc.Graph(id='length-time-chart')
    ])

    app.run_server(debug=True)

if __name__ == '__main__':
    main()
