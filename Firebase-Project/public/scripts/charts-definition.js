// Create Current Chart
function createCurrentChart() {
    var chart = new Highcharts.Chart({
      time:{
        useUTC: false
      },
      chart:{ 
        renderTo:'chart-current',
        type: 'spline' 
      },
      series: [
        {
          name: 'CTL-16-CLS'
        }
      ],
      title: { 
        text: undefined
      },
      plotOptions: {
        line: { 
          animation: false,
          dataLabels: { 
            enabled: true 
          }
        }
      },
      xAxis: {
        type: 'datetime',
        dateTimeLabelFormats: { second: '%H:%M:%S' }
      },
      yAxis: {
        title: { 
          text: 'Current Ampere' 
        }
      },
      credits: { 
        enabled: false 
      }
    });
    return chart;
  }
  
  // Create Power Chart
  function createPowerChart(){
    var chart = new Highcharts.Chart({
      time:{
        useUTC: false
      },
      chart:{ 
        renderTo:'chart-power',
        type: 'spline'  
      },
      series: [{
        name: 'CTL-16-CLS'
      }],
      title: { 
        text: undefined
      },    
      plotOptions: {
        line: { 
          animation: false,
          dataLabels: { 
            enabled: true 
          }
        },
        series: { 
          color: '#50b8b4' 
        }
      },
      xAxis: {
        type: 'datetime',
        dateTimeLabelFormats: { second: '%H:%M:%S' }
      },
      yAxis: {
        title: { 
          text: 'Power (Watt)' 
        }
      },
      credits: { 
        enabled: false 
      }
    });
    return chart;
  }
  
  function createTimeRangeChart(data) {
    Highcharts.chart('chart-time', {
        title: {
            text: 'Sensor Readings Over Time'
        },
        xAxis: {
            categories: data.labels,  // timestamps
            title: {
                text: 'Time'
            }
        },
        yAxis: {
            title: {
                text: 'Sensor Value'
            }
        },
        series: [{
            name: 'Sensor Reading',
            data: data.datasets[0].data
        }]
    });
}

// Create Time Chart
function createTimeChart(){
  var chart = new Highcharts.Chart({
    time:{
      useUTC: false
    },
    chart:{ 
      renderTo:'chart-time',
      type: 'spline'  
    },
    series: [{
      name: 'CTL-16-CLS'
    }],
    title: { 
      text: undefined
    },    
    plotOptions: {
      line: { 
        animation: false,
        dataLabels: { 
          enabled: true 
        }
      },
      series: { 
        color: '#50b8b4' 
      }
    },
    xAxis: {
      type: 'datetime',
      dateTimeLabelFormats: { second: '%H:%M:%S' }
    },
    yAxis: {
      title: { 
        text: 'Current (Ampere)' 
      }
    },
    credits: { 
      enabled: false 
    }
  });
  return chart;
}