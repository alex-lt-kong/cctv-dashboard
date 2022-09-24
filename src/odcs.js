import * as React from 'react';
import ReactDOM from 'react-dom';
import axios from 'axios';
import Grid from '@mui/material/Grid'; // Grid version 1
import AppBar from '@mui/material/AppBar';
import Box from '@mui/material/Box';
import Toolbar from '@mui/material/Toolbar';
import Typography from '@mui/material/Typography';
import Card from '@mui/material/Card';
import Button from '@mui/material/Button';
import GasMeterIcon from '@mui/icons-material/GasMeter';
import ImageList from '@mui/material/ImageList';
import ImageListItem from '@mui/material/ImageListItem';
import { ThemeProvider, createTheme } from '@mui/material/styles';

class LiveImages extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      cctvEndpoint: './cctv/?device_id=',
      cctvList: [],
      randNums: []
    };
    this.timer = 0;
    this.startTimer = this.startTimer.bind(this);
    this.countDown = this.countDown.bind(this);
  }

  componentDidMount() {
    axios.get('./get_device_count_json/')
    .then((response) => {
      console.log(response.data);
      this.setState({
        cctvList: Array.from(Array(response.data.data).keys()),
        randNums: Array.from(Array(response.data.data).keys())
      });
    })
    .catch((error) => {
      console.error(error);
      alert(`${error}`);
    });

    
  }

  startTimer() {
    if (this.timer === 0 && this.state.seconds > 0) {
      this.timer = setInterval(this.countDown, 1000);
    }
  }

  countDown() {
    // Remove one second, set state so a re-render happens.
    const seconds = this.state.seconds - 1;
    this.setState({
      seconds: seconds
    });
    // Check if we're at zero.
    if (seconds === 0) {
      clearInterval(this.timer);
      this.timer = 0;
    }
  }

  render() {


    const theme = createTheme({
      breakpoints: {
        values: {
          mobile: 0,
          tablet: 768,
          desktop: 1024
        }
      }
    });
    return (
      <ThemeProvider theme={theme}>
      <Box
        sx={{
          display: "grid",
          gridTemplateColumns: {
            mobile: "repeat(1, 1fr)",
            tablet: "repeat(2, 1fr)",
            desktop: "repeat(4, 1fr)"
          }
        }}
      >
        {this.state.cctvList.map((idx) => (
          <ImageListItem key={idx}>
            <img
              src={`${this.state.cctvEndpoint}${idx}&${this.state.randNums[idx]}`}
              loading="lazy"
              onLoad={()=>{
                (new Promise((resolve) => setTimeout(resolve, 600))).then(() => {
                  const newRandNums = this.state.randNums;
                  newRandNums[idx] = Math.random();
                  this.setState({randNums: newRandNums});
                });
              }}
            />
          </ImageListItem>
        ))}
      </Box>
        
      </ThemeProvider>
    );
  }
}

class NavBar extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      user: null
    };
  }

  componentDidMount() {
    axios.get('../get_logged_in_user_json/')
        .then((response) => {
          this.setState({
            user: response.data.data
          });
        })
        .catch((error) => {
          console.error(error);
          alert(`${error}`);
        });
  }

  render() {
    return (
      <Box sx={{flexGrow: 1, mb: '1rem'}}>
        <AppBar position="static">
          <Toolbar>
            <GasMeterIcon sx={{display: {md: 'flex'}, mr: 1}} />
            <Typography
              variant="h6"
              noWrap
              component="a"
              href="/"
              sx={{
                mr: 2,
                display: {md: 'flex'},
                fontFamily: 'monospace',
                fontWeight: 700,
                letterSpacing: '.1rem',
                color: 'inherit',
                textDecoration: 'none'
              }}
            >
              CCTV
            </Typography>
            <Typography variant="h6" component="div" sx={{flexGrow: 1}}>
            </Typography>
            <Button color="inherit">{this.state.user}</Button>
          </Toolbar>
        </AppBar>
      </Box>
    );
  }
}

class Index extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      currDate: new Date()
    };
  }

  render() {
    return (
      <>
        <NavBar />
        <div style={{maxWidth: '1440px', display: 'block', marginLeft: 'auto', marginRight: 'auto'}}>
          <LiveImages />          
        </div>
      </>
    );
  }
}

const container = document.getElementById('root');
ReactDOM.render(<Index />, container);
