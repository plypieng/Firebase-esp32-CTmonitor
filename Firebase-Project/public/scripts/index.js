// convert epochtime to JavaScripte Date object
function epochToJsDate(epochTime){
  return new Date(epochTime);
}

// convert time to human-readable format YYYY/MM/DD HH:MM:SS
function epochToDateTime(epochTime){
  var epochDate = new Date(epochTime);
  var dateTime = epochDate.getFullYear() + "/" +
    ("00" + (epochDate.getMonth() + 1)).slice(-2) + "/" +
    ("00" + epochDate.getDate()).slice(-2) + " " +
    ("00" + epochDate.getHours()).slice(-2) + ":" +
    ("00" + epochDate.getMinutes()).slice(-2) + ":" +
    ("00" + epochDate.getSeconds()).slice(-2);
  return dateTime;
}

// function to plot values on charts
function plotValues(chart, timestamp, value){
  var x = epochToJsDate(timestamp).getTime();
  var y = Number (value);
  if(chart.series[0].data.length > 30) {
    chart.series[0].addPoint([x, y], true, true, true);
  } else {
    chart.series[0].addPoint([x, y], true, false, true);
  }
}



// DOM elements
const logoutNavElement = document.querySelector('#logout-link');
const loginNavElement = document.querySelector('#login-link');
const signedOutMessageElement = document.querySelector('#signedOutMessage');
const dashboardElement = document.querySelector("#charts-container"); //#dashboardSignedIn
const userDetailsElement = document.querySelector('#user-details');

// Elements for sensor readings
const currentCardElement = document.getElementById("current");
const powerCardElement = document.getElementById("power");
const lastUpdateElement = document.getElementById('last-update');
const tableContainerElement = document.querySelector('#table-container');

const timeRangeChartElement = document.querySelector('#time-charts-container');

//Buttons
const loadDataButtonElement = document.getElementById('load-data');
const viewDataButtonElement = document.getElementById('view-data-button');
const hideDataButtonElement = document.getElementById('hide-data-button'); 
const deleteDataElement = document.querySelector('#delete-data');



// MANAGE LOGIN/LOGOUT UI
const setupUI = (user) => {
if (user) {
  //toggle UI elements
  logoutNavElement.style.display = 'block';
  loginNavElement.style.display = 'none';
  signedOutMessageElement.style.display ='none';
  dashboardElement.style.display = 'block';
  userDetailsElement.style.display ='block';
  userDetailsElement.innerHTML = user.email;
  tableContainerElement.style.display = 'none';

  // get user UID to get data from database
  var uid = user.uid;
  console.log(uid);

  // Database paths (with user UID)
  var dbPathReadings = 'UsersData/' + uid.toString() + '/sensor';

  //////// SENSOR READINGS ////////

  //Reference to the parent node where the readings are saved
  var dbReadingsRef = firebase.database().ref(dbPathReadings);

  // Get the latest readings and display on cards
  dbReadingsRef.limitToLast(1).on('child_added', snapshot =>{
    var jsonData = snapshot.toJSON(); // example: {current: 100.23, power: 2000.20, timestamp:1641317355}
    //console.log("Cards Display");
    //console.log(jsonData);
    var current = jsonData.current;
    var power = jsonData.power;
    var timestamp = jsonData.timestamp;
    // Update DOM elements
    currentCardElement.innerHTML = current;
    powerCardElement.innerHTML = power;
    lastUpdateElement.innerHTML = epochToDateTime(timestamp);
  });

  // Render charts to display data
  chartC = createCurrentChart();
  chartP = createPowerChart();
  //Get the latest 30 readings to display on charts
  dbReadingsRef.orderByKey().limitToLast(30).on('child_added', snapshot =>{
    var jsonData = snapshot.toJSON(); // example: {current: 100.23, power: 2000.20, timestamp:1641317355}
    //console.log(jsonData);
    // Save values on variables
    var current = jsonData.current;
    var power = jsonData.power;
    var timestamp = jsonData.timestamp;
    // Plot the values on charts
    plotValues(chartC, timestamp, current);
    plotValues(chartP, timestamp, power);
  });


   // Table
   var lastID; //saves the pushID of the last readings displayed on the table
   function createTable(){
     // Append all data to the table
     var firstRun = true;
     dbReadingsRef.orderByKey().limitToLast(50).on('child_added', function(snapshot) {
       if (snapshot.exists()) {
          var jsonData = snapshot.toJSON();
          //console.log(jsonData);
          var current = jsonData.current;
          var power = jsonData.power;
          var timestamp = jsonData.timestamp;
          var content = '';
          content += '<tr>';
          content += '<td>' + epochToDateTime(timestamp) + '</td>';
          content += '<td>' + current + '</td>';
          content += '<td>' + power + '</td>';
          content += '</tr>';
         $('#table-body').prepend(content);
         // Save lastID --> corresponds to the first key on the returned snapshot data
         if (firstRun){
           lastID = snapshot.key;
           firstRun=false;
           //console.log(lastID);
         }
       }
     });
   };


   // Button that shows the table
  viewDataButtonElement.addEventListener('click', (e) =>{
      // Toggle DOM elements
      tableContainerElement.style.display = 'block';
      viewDataButtonElement.style.display ='none';
      hideDataButtonElement.style.display ='inline-block';
      loadDataButtonElement.style.display = 'inline-block'
      createTable();
    }); 

  // Append readings to table (after pressing More results... button)
  function appendToTable(){
      var dataList = []; // saves list of readings returned by the snapshot (oldest-->newest)
      var reversedList = []; // the same as previous, but reversed (newest--> oldest)
      console.log("APEND");
      dbReadingsRef.orderByKey().limitToLast(50).endAt(String(lastID)).once('value', function(snapshot) {
        //console.log(snapshot);
        // Convert the snapshot to JSON
        if (snapshot.exists()) {
          var firstRun = true;
          snapshot.forEach(element => {
            // Get the last pushID (it's the first on the snapshot oldest data --> newest data)
            if (firstRun){
              lastID = element.key
              //console.log("last reading");
              //console.log(lastID);
              firstRun = false;
            }
            var jsonData = element.toJSON();
            //console.log(jsonData);
            dataList.push(jsonData); // create a list with all data
            //console.log(dataList);
          });
          reversedList = dataList.reverse(); // reverse the order of the list (newest data --> oldest data)

          var firstTime = true;
          // loop through all elements of the list and append to table (newest elements first)
          reversedList.forEach(element =>{
            if (firstTime){ // ignore first reading (it's already on the table from the previous query)
              firstTime = false;
            }
            else{ //append the new readings to the table
              var current = element.current;
              var power = element.power;
              var timestamp = element.timestamp;
              var content = '';
              content += '<tr>';
              content += '<td>' + epochToDateTime(timestamp) + '</td>';
              content += '<td>' + current + '</td>';
              content += '<td>' + power + '</td>';
              content += '</tr>';
              $('#table-body').append(content);
            }
          });
        }
      });
    }

     // When you click on the more results... button, call the appendToTable function
      loadDataButtonElement.addEventListener('click', (e) => {
          appendToTable();
      });

      // Hide the table when you click on Hide Data
      hideDataButtonElement.addEventListener('click', (e) => {
          tableContainerElement.style.display = 'none';
          viewDataButtonElement.style.display = 'inline-block';
          hideDataButtonElement.style.display = 'none';
      });

      deleteDataElement.addEventListener('click', (e) => {
      // delete data (readings)
          dbReadingsRef.remove();
          window.location.reload();
      });
      

      

      
      







      document.getElementById('generate-chart').addEventListener('click', function() {
        const startTime = new Date(document.getElementById('start_time').value).getTime();
        const endTime = new Date(document.getElementById('end_time').value).getTime();
    
        // Validate if the entered times are valid
        if (!startTime || !endTime || startTime >= endTime) {
            alert("Please enter a valid start and end time.");
            return;
        }
    

        chartT = createTimeChart();
      //Get the latest 30 readings to display on charts
      dbReadingsRef.orderByKey().on('child_added', snapshot =>{
        var jsonData = snapshot.toJSON(); // example: {current: 100.23, power: 2000.20, timestamp:1641317355}
        //console.log(jsonData);
        // Save values on variables
        var current = jsonData.current;
        var timestamp = jsonData.timestamp;
        // Plot the values on charts
        plotValues(chartT, timestamp, current);
      });
        let chartData = {
            labels: [],  // timestamps will go here
            datasets: [{
                label: 'Sensor Readings',
                data: [],  // sensor values will go here
            }]
        };
    
        // Fetch data from Firebase for the specified time range
        dbReadingsRef.orderByChild('timestamp').startAt(startTime).endAt(endTime).once('value')
            .then(snapshot => {
                snapshot.forEach(childSnapshot => {
                    let reading = childSnapshot.val();
                    chartData.labels.push(epochToDateTime(reading.timestamp));
                    chartData.datasets[0].data.push(reading.current);  // assuming you want to plot 'current' value
                });
    
                // Render the chart using Highcharts
                createTimeRangeChart(chartData);
            })
            .catch(err => {
                console.error("Error fetching data:", err);
            });
      });








// if user is logged out
} else{
  // toggle UI elements
  logoutNavElement.style.display = 'none';
  loginNavElement.style.display = 'block';
  signedOutMessageElement.style.display ='block';
  dashboardElement.style.display = 'block';
  userDetailsElement.style.display ='none';
}
}