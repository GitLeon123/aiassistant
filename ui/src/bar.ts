import { mount } from 'svelte';
import Bar from './bar/Bar.svelte';
import { connect } from './lib/bridge';
import './lib/state.svelte';
import './styles/base.css';

const app = mount(Bar, { target: document.getElementById('app')! });
void connect('bar');

export default app;
